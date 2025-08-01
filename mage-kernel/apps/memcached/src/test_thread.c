#include "test_config.h"
#include "test_thread.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>

static bool allow_closed_read = false;
static const char *data_str = "deadbeaf";
static const char *key_prefix = "memtier-";

static struct test_settings ts_array[MAX_TEST_THREADS];
static uint64_t latency_array[MAX_TEST_THREADS * N_TEST_REQS];
static cpu_set_t cpu_set[MAX_TEST_THREADS]; 

/* thread-specific variable for deeply finding the item lock type */
static pthread_key_t item_lock_type_key;

/* Barrier */
static void barrier_init(struct barrier *b, int n)
{
  pthread_cond_init(&b->complete, NULL);
  pthread_mutex_init(&b->mutex, NULL);
  b->count = n;
  b->crossing = 0;
}

static void barrier_cross(struct barrier *b)
{
  pthread_mutex_lock(&b->mutex);
  /* One more thread through */
  b->crossing++;
  /* If not all here, wait */
  if (b->crossing < b->count) {
    pthread_cond_wait(&b->complete, &b->mutex);
  } else {
    pthread_cond_broadcast(&b->complete);
    /* Reset for next time */
    b->crossing = 0;
  }
  pthread_mutex_unlock(&b->mutex);
}

/* Random Generator */
static inline int MarsagliaXORV (int x) { 
  if (x == 0) x = 1 ; 
  x ^= x << 6;
  x ^= ((unsigned)x) >> 21;
  x ^= x << 7 ; 
  return x ;        // use either x or x & 0x7FFFFFFF
}

static inline int MarsagliaXOR (int * seed) {
  int x = MarsagliaXORV(*seed);
  *seed = x ; 
  return x & 0x7FFFFFFF;
}


static inline void rand_init(unsigned short *seed)
{
  seed[0] = (unsigned short)rand();
  seed[1] = (unsigned short)rand();
  seed[2] = (unsigned short)rand();
}

static inline int rand_range(int n, unsigned short *seed)
{
  /* Return a random number in range [0;n) */
  
  /*int v = (int)(erand48(seed) * n);
  assert (v >= 0 && v < n);*/
  
  int v = MarsagliaXOR((int *)seed) % n;
  return v;
}

/* Helper function for qsort */
int compare_uint64_t(const void *a, const void *b) {
    uint64_t uint_a = *((const uint64_t *)a);
    uint64_t uint_b = *((const uint64_t *)b);
    
    if (uint_a < uint_b) return 1;
    if (uint_a > uint_b) return -1;
    return 0;
}

/* Send and Receive */
static void safe_send(int sock, const void* buf, size_t len, bool hickup)
{
    off_t offset = 0;
    const char* ptr = buf;
#ifdef MESSAGE_DEBUG
    uint8_t val = *ptr;
    assert(val == (uint8_t)0x80);
    fprintf(stderr, "About to send %lu bytes:", (unsigned long)len);
    for (int ii = 0; ii < len; ++ii) {
        if (ii % 4 == 0) {
            fprintf(stderr, "\n   ");
        }
        val = *(ptr + ii);
        fprintf(stderr, " 0x%02x", val);
    }
    fprintf(stderr, "\n");
    usleep(500);
#endif

    do {
        size_t num_bytes = len - offset;
        if (hickup) {
            if (num_bytes > 1024) {
                num_bytes = (rand() % 1023) + 1;
            }
        }

        ssize_t nw = write(sock, ptr + offset, num_bytes);
        if (nw == -1) {
            if (errno != EINTR) {
                fprintf(stderr, "Failed to write: %s\n", strerror(errno));
                abort();
            }
        } else {
            if (hickup) {
                usleep(100);
            }
            offset += nw;
        }
    } while (offset < len);
}

static bool safe_recv(int sock, void *buf, size_t len) {
    if (len == 0) {
        return true;
    }
    off_t offset = 0;
    do {
        ssize_t nr = read(sock, ((char*)buf) + offset, len - offset);
        if (nr == -1) {
            if (errno != EINTR) {
                fprintf(stderr, "Failed to read: %s\n", strerror(errno));
                abort();
            }
        } else {
            if (nr == 0 && allow_closed_read) {
                return false;
            }
            assert(nr != 0);
            offset += nr;
        }
    } while (offset < len);

    return true;
}

static bool safe_recv_packet(int sock, void *buf, size_t size) {
    protocol_binary_response_no_extras *response = buf;
    assert(size > sizeof(*response));
    if (!safe_recv(sock, response, sizeof(*response))) {
        return false;
    }
    response->message.header.response.keylen = ntohs(response->message.header.response.keylen);
    response->message.header.response.status = ntohs(response->message.header.response.status);
    response->message.header.response.bodylen = ntohl(response->message.header.response.bodylen);

    size_t len = sizeof(*response);

    char *ptr = buf;
    ptr += len;
    if (!safe_recv(sock, ptr, response->message.header.response.bodylen)) {
        return false;
    }

#ifdef MESSAGE_DEBUG
    usleep(500);
    ptr = buf;
    len += response->message.header.response.bodylen;
    uint8_t val = *ptr;
    assert(val == (uint8_t)0x81);
    fprintf(stderr, "Received %lu bytes:", (unsigned long)len);
    for (int ii = 0; ii < len; ++ii) {
        if (ii % 4 == 0) {
            fprintf(stderr, "\n   ");
        }
        val = *(ptr + ii);
        fprintf(stderr, " 0x%02x", val);
    }
    fprintf(stderr, "\n");
#endif
    return true;
}

static off_t storage_command(char*buf,
                             size_t bufsz,
                             uint8_t cmd,
                             const void* key,
                             size_t keylen,
                             const void* dta,
                             size_t dtalen,
                             uint32_t flags,
                             uint32_t exp) {
    /* all of the storage commands use the same command layout */
    protocol_binary_request_set *request = (void*)buf;
    assert(bufsz > sizeof(*request) + keylen + dtalen);

    memset(request, 0, sizeof(*request));
    request->message.header.request.magic = PROTOCOL_BINARY_REQ;
    request->message.header.request.opcode = cmd;
    request->message.header.request.keylen = htons(keylen);
    request->message.header.request.extlen = 8;
    request->message.header.request.bodylen = htonl(keylen + 8 + dtalen);
    request->message.header.request.opaque = 0xdeadbeef;
    request->message.body.flags = flags;
    request->message.body.expiration = exp;

    off_t key_offset = sizeof(protocol_binary_request_no_extras) + 8;

    memcpy(buf + key_offset, key, keylen);
    if (dta != NULL) {
        memcpy(buf + key_offset + keylen, dta, dtalen);
    }

    return key_offset + keylen + dtalen;
}

static off_t raw_command(char* buf,
                         size_t bufsz,
                         uint8_t cmd,
                         const void* key,
                         size_t keylen,
                         const void* dta,
                         size_t dtalen) {
    /* all of the storage commands use the same command layout */
    protocol_binary_request_no_extras *request = (void*)buf;
    assert(bufsz > sizeof(*request) + keylen + dtalen);

    memset(request, 0, sizeof(*request));
    request->message.header.request.magic = PROTOCOL_BINARY_REQ;
    request->message.header.request.opcode = cmd;
    request->message.header.request.keylen = htons(keylen);
    request->message.header.request.bodylen = htonl(keylen + dtalen);
    request->message.header.request.opaque = 0xdeadbeef;

    off_t key_offset = sizeof(protocol_binary_request_no_extras);

    if (key != NULL) {
        memcpy(buf + key_offset, key, keylen);
    }
    if (dta != NULL) {
        memcpy(buf + key_offset + keylen, dta, dtalen);
    }

    return sizeof(*request) + keylen + dtalen;
}

static void validate_response_header(protocol_binary_response_no_extras *response,
                                     uint8_t cmd, uint16_t status)
{
    assert(response->message.header.response.magic == PROTOCOL_BINARY_RES);
    assert(response->message.header.response.opcode == cmd);
    assert(response->message.header.response.datatype == PROTOCOL_BINARY_RAW_BYTES);
    assert(response->message.header.response.status == status);
    assert(response->message.header.response.opaque == 0xdeadbeef);

    if (status == PROTOCOL_BINARY_RESPONSE_SUCCESS) {
        switch (cmd) {
        case PROTOCOL_BINARY_CMD_ADDQ:
        case PROTOCOL_BINARY_CMD_APPENDQ:
        case PROTOCOL_BINARY_CMD_DECREMENTQ:
        case PROTOCOL_BINARY_CMD_DELETEQ:
        case PROTOCOL_BINARY_CMD_FLUSHQ:
        case PROTOCOL_BINARY_CMD_INCREMENTQ:
        case PROTOCOL_BINARY_CMD_PREPENDQ:
        case PROTOCOL_BINARY_CMD_QUITQ:
        case PROTOCOL_BINARY_CMD_REPLACEQ:
        case PROTOCOL_BINARY_CMD_SETQ:
            assert("Quiet command shouldn't return on success" == NULL);
        default:
            break;
        }

        switch (cmd) {
        case PROTOCOL_BINARY_CMD_ADD:
        case PROTOCOL_BINARY_CMD_REPLACE:
        case PROTOCOL_BINARY_CMD_SET:
        case PROTOCOL_BINARY_CMD_APPEND:
        case PROTOCOL_BINARY_CMD_PREPEND:
            assert(response->message.header.response.keylen == 0);
            assert(response->message.header.response.extlen == 0);
            assert(response->message.header.response.bodylen == 0);
            assert(response->message.header.response.cas != 0);
            break;
        case PROTOCOL_BINARY_CMD_FLUSH:
        case PROTOCOL_BINARY_CMD_NOOP:
        case PROTOCOL_BINARY_CMD_QUIT:
        case PROTOCOL_BINARY_CMD_DELETE:
            assert(response->message.header.response.keylen == 0);
            assert(response->message.header.response.extlen == 0);
            assert(response->message.header.response.bodylen == 0);
            assert(response->message.header.response.cas == 0);
            break;

        case PROTOCOL_BINARY_CMD_DECREMENT:
        case PROTOCOL_BINARY_CMD_INCREMENT:
            assert(response->message.header.response.keylen == 0);
            assert(response->message.header.response.extlen == 0);
            assert(response->message.header.response.bodylen == 8);
            assert(response->message.header.response.cas != 0);
            break;

        case PROTOCOL_BINARY_CMD_STAT:
            assert(response->message.header.response.extlen == 0);
            /* key and value exists in all packets except in the terminating */
            assert(response->message.header.response.cas == 0);
            break;

        case PROTOCOL_BINARY_CMD_VERSION:
            assert(response->message.header.response.keylen == 0);
            assert(response->message.header.response.extlen == 0);
            assert(response->message.header.response.bodylen != 0);
            assert(response->message.header.response.cas == 0);
            break;

        case PROTOCOL_BINARY_CMD_GET:
        case PROTOCOL_BINARY_CMD_GETQ:
            assert(response->message.header.response.keylen == 0);
            assert(response->message.header.response.extlen == 4);
            assert(response->message.header.response.cas != 0);
            break;

        case PROTOCOL_BINARY_CMD_GETK:
        case PROTOCOL_BINARY_CMD_GETKQ:
            assert(response->message.header.response.keylen != 0);
            assert(response->message.header.response.extlen == 4);
            assert(response->message.header.response.cas != 0);
            break;

        default:
            /* Undefined command code */
            break;
        }
    } else {
        assert(response->message.header.response.cas == 0);
        assert(response->message.header.response.extlen == 0);
        if (cmd != PROTOCOL_BINARY_CMD_GETK &&
            cmd != PROTOCOL_BINARY_CMD_GATK) {
            assert(response->message.header.response.keylen == 0);
        }
    }
}

static struct addrinfo *lookuphost(const char *hostname, in_port_t port)
{
    struct addrinfo *ai = 0;
    struct addrinfo hints = { .ai_family = AF_UNSPEC,
                              .ai_protocol = IPPROTO_TCP,
                              .ai_socktype = SOCK_STREAM };
    char service[NI_MAXSERV];
    int error;

    (void)snprintf(service, NI_MAXSERV, "%d", port);
    if ((error = getaddrinfo(hostname, service, &hints, &ai)) != 0) {
       if (error != EAI_SYSTEM) {
          fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(error));
       } else {
          perror("getaddrinfo()");
       }
    }

    return ai;
}

static int connect_server(const char *hostname, in_port_t port, bool nonblock)
{
    struct addrinfo *ai = lookuphost(hostname, port);
    int sock = -1;
    if (ai != NULL) {
       if ((sock = socket(ai->ai_family, ai->ai_socktype,
                          ai->ai_protocol)) != -1) {
          if (connect(sock, ai->ai_addr, ai->ai_addrlen) == -1) {
             fprintf(stderr, "Failed to connect socket: %s\n",
                     strerror(errno));
             close(sock);
             sock = -1;
          } else if (nonblock) {
              int flags = fcntl(sock, F_GETFL, 0);
              if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
                  fprintf(stderr, "Failed to enable nonblocking mode: %s\n",
                          strerror(errno));
                  close(sock);
                  sock = -1;
              }
          }
       } else {
          fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
       }

       freeaddrinfo(ai);
    }
    return sock;
}

static void test_parallel(struct test_settings *ts, int sock){
    assert(ts->key_size <= MAX_TEST_KEY_LEN);
    assert(ts->data_size <= MAX_TEST_DATA_LEN); 
    int ii = 0;
    int key_len = 0;
    int orig_key_len = 0;
    size_t idx_base = ts->tid * ts ->n_requests;
    size_t len = 0;
    union {
        protocol_binary_request_no_extras request;
        protocol_binary_response_no_extras response;
        char bytes[DEFAULT_BUFFER_SIZE];
    } send, receive;
    char key_buffer[MAX_TEST_KEY_LEN + 1];
    char data_buffer[MAX_TEST_DATA_LEN + 1];

    /* Prepare data */
    for (ii = 0; ii + 8 < ts->data_size; ii+=8){
        strcpy(data_buffer + ii, data_str);
    }
    for (; ii < ts->data_size; ii++){
        data_buffer[ii] = 'a'; 
    }
    data_buffer[ii] = '\0';
        
    key_len = snprintf(key_buffer, sizeof(key_buffer), "%s%08lu", 
        key_prefix, idx_base); 
    orig_key_len = key_len;
    len = storage_command(send.bytes, sizeof(send.bytes), PROTOCOL_BINARY_CMD_SET,
                                key_buffer, key_len, data_buffer, ts->data_size,
                                0, 0);
    //printf("%lu %lu %d %d %d %s\n", len, idx_base, ts->tid, ii, key_len, key_buffer);

    /* Start Bar */
    barrier_cross(ts->b);
    for (ii = 0; ii < ts->n_requests; ii++){
        /* Key sequentially from tid * n_requests to (tid + 1) * n_requests */
        key_len = snprintf(key_buffer, sizeof(key_buffer), "%s%08lu", 
            key_prefix, idx_base + (size_t)ii); 
        assert(key_len == orig_key_len);
        memcpy(send.bytes + sizeof(protocol_binary_request_no_extras) + 8, key_buffer, key_len);        
        safe_send(sock, send.bytes, len, false);
        safe_recv_packet(sock, receive.bytes, sizeof(receive.bytes));
        validate_response_header(&receive.response, PROTOCOL_BINARY_CMD_SET,
                                    PROTOCOL_BINARY_RESPONSE_SUCCESS);
    }
}

static void test_zipfian(struct test_settings *ts, int sock){
    /* Not implemented yet */ 
    assert(ts->key_size <= MAX_TEST_KEY_LEN);
    assert(ts->data_size <= MAX_TEST_DATA_LEN); 
    int ii = 0;
    int key_len = 0;
    int orig_key_len = 0;
    size_t idx_base = ts->tid * ts ->n_requests;
    size_t len = 0;
    size_t get_len = 0;
    size_t idx = 0;
    protocol_binary_command op = PROTOCOL_BINARY_CMD_GET;
    item *it = NULL;
    struct timeval start, end;
    union {
        protocol_binary_request_no_extras request;
        protocol_binary_response_no_extras response;
        char bytes[DEFAULT_BUFFER_SIZE];
    } send, receive, send_get;
    char key_buffer[MAX_TEST_KEY_LEN + 1];
    char data_buffer[MAX_TEST_DATA_LEN + 1];

    /* Prepare data */
    for (ii = 0; ii + 8 < ts->data_size; ii+=8){
        strcpy(data_buffer + ii, data_str);
    }
    for (; ii < ts->data_size; ii++){
        data_buffer[ii] = 'a'; 
    }
    data_buffer[ii] = '\0';
        
    key_len = snprintf(key_buffer, sizeof(key_buffer), "%s%08lu", 
        key_prefix, idx_base); 
    orig_key_len = key_len;
    len = storage_command(send.bytes, sizeof(send.bytes), PROTOCOL_BINARY_CMD_SET,
                                key_buffer, key_len, data_buffer, ts->data_size,
                                0, 0);

    get_len = raw_command(send_get.bytes, sizeof(send_get.bytes), PROTOCOL_BINARY_CMD_GET,
                        key_buffer, key_len, NULL, 0);
    
    //printf("Starting the benchmark for %d sleep time.\n", SLEEP_TIME);
    /* Start Bar */
    barrier_cross(ts->b);
    for (ii = 0; ii < ts->n_requests; ii++){
        /* Generate Index following zipfian */
        // usleep(SLEEP_TIME);
        /*
        for (unsigned long k = 0; k < SLEEP_TIME * 1000; k++){
            asm volatile("nop");
            asm volatile("nop");
            asm volatile("nop");
            asm volatile("nop");
            asm volatile("nop");
        }
        */
    
        idx = zipf_next(&ts->zs);

        /* Generate Op */
        op = rand_range(1000, ts->seed) < ts->write_ratio ? PROTOCOL_BINARY_CMD_SET : PROTOCOL_BINARY_CMD_GET;

        /* Dispatch action */
        key_len = snprintf(key_buffer, sizeof(key_buffer), "%s%08lu", 
            key_prefix, idx); 
        //if (ts->tid == 0)
        //    printf("%lu %lu %d %d %d %s\n", len, idx_base, ts->tid, ii, key_len, key_buffer);
        assert(key_len == orig_key_len);

        if (op == PROTOCOL_BINARY_CMD_GET){
            memcpy(send_get.bytes + sizeof(protocol_binary_request_no_extras), key_buffer, key_len);        
            #ifdef LATENCY_TEST
            if (ii % LATENCY_SAMPLE_GAP == 0){
                gettimeofday(&start, NULL);
            }
            #endif
            /* For get, we use some hack to temporarily circumvent the whole inefficient network stack of OSv */
            it = item_get(key_buffer, key_len);
            if (it) {
                item_update(it);
            }
            //safe_send(sock, send_get.bytes, get_len, false);
            //safe_recv_packet(sock, receive.bytes, sizeof(receive.bytes));
            #ifdef LATENCY_TEST
            if (ii % LATENCY_SAMPLE_GAP == 0){
                gettimeofday(&end, NULL);
                latency_array[ts->n_requests + ii / LATENCY_SAMPLE_GAP] = \
                    (end.tv_sec * 1000 * 1000 + end.tv_usec) - (start.tv_sec * 1000 * 1000 + start.tv_usec); 
            }
            #endif

        } else if (op == PROTOCOL_BINARY_CMD_SET){
            memcpy(send.bytes + sizeof(protocol_binary_request_no_extras) + 8, key_buffer, key_len);        
            #ifdef LATENCY_TEST
            if (ii % LATENCY_SAMPLE_GAP == 0){
                gettimeofday(&start, NULL);
            }
            #endif
            safe_send(sock, send.bytes, len, false);
            safe_recv_packet(sock, receive.bytes, sizeof(receive.bytes));
            #ifdef LATENCY_TEST
            if (ii % LATENCY_SAMPLE_GAP == 0){
                gettimeofday(&end, NULL);
                latency_array[ts->n_requests + ii / LATENCY_SAMPLE_GAP] = \
                    (end.tv_sec * 1000 * 1000 + end.tv_usec) - (start.tv_sec * 1000 * 1000 + start.tv_usec); 
            }
            #endif
        }
    }
}

static void* test_thread_worker(void *arg){
    int sock;
    struct test_settings *ts = arg;

    sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set[ts->tid]);
    pthread_setspecific(item_lock_type_key, &ts->item_lock_type);
    
    /* Connect to the memcached via lo0 */
    sock = connect_server("127.0.0.1", ts->test_port, false);
    if (sock == -1){
        fprintf(stderr, "test thread failed\n");
        exit(1);        
    }

    /* Request generation */
    switch (ts->pattern) {
        case access_parallel:
            /* For now 100% set */
            test_parallel(ts, sock);
            break;
        case access_zipfian:
            test_zipfian(ts, sock);
            break;
        default:
            fprintf(stderr, "Unknown access pattern: %d\n", ts->pattern);
            exit(1);
            break;
    }

    /* Disconnect to the memcached */
    close(sock);

    return NULL;
}

/*
 * Here the work flow is as follows:
 * - Create a main thread and then return
 * - The main thread wait for 5 seconds for the event loop to start
 * - The main thread creates a bunch of filling-up threads
 *      - Each filling-up threads works as follows:
 *      - Create a connection
 *      - Parallel set a region to fill 20G space
 *      - Clean-up the connection
 * - The main thread then joins all the filling-up threads
 * - The main thread creates a bunch of test threads
 *      - Each test thread works as follows:
 *      - Create a connection
 *      - Send set/get requests in a closed loop
 *      - Clean-up the connection
 * - The main thread joins the test threads and reports the result 
 */
static void* main_test_thread(void *arg){
    int ii = 0;
    int ret = 0;
    uint64_t global_offset = 0;
    uint64_t duration = 0;
    unsigned int local_seed = 0;
    struct test_settings *ts = NULL;
    struct main_test_arg *main_arg = arg;
    struct barrier bar;
    struct timeval start, end;
    pthread_t test_worker_pids[MAX_TEST_THREADS];
    pthread_attr_t test_worker_attrs[MAX_TEST_THREADS];

    assert(main_arg->n_test_threads <= MAX_TEST_THREADS);

    /* Initialization */
    srand(time(NULL));
    barrier_init(&bar, main_arg->n_test_threads + 1);
    usleep(5000);
    global_offset = (uint64_t)rand();

    /* Create a bunch of filling-up threads */
    /* Remember to initialize seed */
    for (ii = 0; ii < main_arg->n_test_threads; ii ++){
        pthread_attr_init(&test_worker_attrs[ii]);
        pthread_attr_setdetachstate(&test_worker_attrs[ii], PTHREAD_CREATE_JOINABLE);
        CPU_ZERO(&cpu_set[ii]);
        CPU_SET(NUMA_CPUS + ii, &cpu_set[ii]);
        ts = &ts_array[ii];

        /* Initialize ts */
        ts -> n_test_threads = main_arg->n_test_threads;
        ts -> tid = ii;
        ts -> write_ratio = 1000; /* Unused for parallel */
        ts -> n_requests = N_FILL_REQS;
        ts -> key_size = MAX_TEST_KEY_LEN;
        ts -> data_size = MAX_TEST_DATA_LEN;
        ts -> test_port = main_arg ->s->port;
        ts -> pattern = access_parallel;
        ts -> zipfian = 0; /* Unused for parallel */
        ts -> b = &bar;
        ts->item_lock_type = ITEM_LOCK_GRANULAR;
        rand_init(ts ->seed);
        /* Skip zs because it is not necessary for parallel */
        
        /* Start the thread. Default stack size is 2MB so no need to worry */
        if ((ret = pthread_create(&test_worker_pids[ii], &test_worker_attrs[ii], 
            test_thread_worker, ts)) != 0) {
            fprintf(stderr, "Can't create test worker thread %d [parallel set]: %s\n",
                    ii, strerror(ret));
            exit(1);
        }
    }

    usleep(2000);

    printf("Start parallel set! \n");
    barrier_cross(&bar); 
    gettimeofday(&start, NULL);
    /* Join*/
    for (ii = 0; ii < main_arg->n_test_threads; ii++){
        printf("Wait for parallel set %d\n", ii);
        if ((ret = pthread_join(test_worker_pids[ii], NULL)) != 0){
            fprintf(stderr, "Can't join test worker thread %d [parallel set]: %s\n",
                    ii, strerror(ret));
            exit(1);
        }
    }
    gettimeofday(&end, NULL);
    printf("Finish parallel set! \n");
    /* In microseconds */
    duration = (end.tv_sec * 1000 * 1000 + end.tv_usec) - (start.tv_sec * 1000 * 1000 + start.tv_usec);
    printf("parallel tput: %f ops\n", ts->n_requests * ts->n_test_threads * 1e6 / duration);



    /* Create a bunch of test threads */
    /* Remember to initialize seed */
    for (ii = 0; ii < main_arg->n_test_threads; ii ++){
        local_seed = ii + (unsigned int)rand();
        pthread_attr_init(&test_worker_attrs[ii]);
        pthread_attr_setdetachstate(&test_worker_attrs[ii], PTHREAD_CREATE_JOINABLE);
        CPU_ZERO(&cpu_set[ii]);
        CPU_SET(NUMA_CPUS + ii, &cpu_set[ii]);
        ts = &ts_array[ii];

        /* Initialize ts */
        ts -> n_test_threads = main_arg->n_test_threads;
        ts -> tid = ii;
        ts -> write_ratio = TEST_WRITE_RATIO;
        ts -> n_requests = N_TEST_REQS;
        ts -> key_size = MAX_TEST_KEY_LEN;
        ts -> data_size = MAX_TEST_DATA_LEN;
        ts -> test_port = main_arg ->s->port;
        ts -> pattern = access_zipfian;
        ts -> zipfian = TEST_ZIPFIAN;
        ts -> b = &bar;
        ts->item_lock_type = ITEM_LOCK_GRANULAR;
        rand_init(ts ->seed);
        zipf_init(&ts->zs, ts->n_test_threads * N_FILL_REQS, ts->zipfian, local_seed);
        zipf_set_rand_off(&ts->zs, global_offset);
        
        /* Start the thread. Default stack size is 2MB so no need to worry */
        if ((ret = pthread_create(&test_worker_pids[ii], &test_worker_attrs[ii], 
            test_thread_worker, ts)) != 0) {
            fprintf(stderr, "Can't create test worker thread %d [zipfian get]: %s\n",
                    ii, strerror(ret));
            exit(1);
        }
    }
    
    usleep(2000);

    printf("Start zipfian get! \n");
    barrier_cross(&bar);
    gettimeofday(&start, NULL);
    /* Join */
    for (ii = 0; ii < main_arg->n_test_threads; ii++){
        printf("Wait for zipfian get %d\n", ii);
        if ((ret = pthread_join(test_worker_pids[ii], NULL)) != 0){
            fprintf(stderr, "Can't join test worker thread %d [zipfian get]: %s\n",
                    ii, strerror(ret));
            exit(1);
        }
    }
    gettimeofday(&end, NULL);
    printf("Finish zipfian get! \n");
    /* In microseconds */
    duration = (end.tv_sec * 1000 * 1000 + end.tv_usec) - (start.tv_sec * 1000 * 1000 + start.tv_usec);
    printf("zipf tput: %f ops\n", ts->n_requests * ts->n_test_threads * 1e6 / duration);

    /* p99 latency */
    /* Sort */
    qsort(latency_array, MAX_TEST_THREADS * N_TEST_REQS, sizeof(uint64_t), compare_uint64_t);
    printf("p99-latency: %lu us\n", latency_array[ts->n_test_threads * ts->n_requests / LATENCY_SAMPLE_GAP / 100]);

    exit(0);
    return NULL;
}

static struct main_test_arg arg;
pthread_t test_thread_init(int n_test_threads, struct settings *s){
    /* Create the main test thread */
    pthread_t       thread;
    pthread_attr_t  attr;
    int             ret;
    
    arg.n_test_threads = n_test_threads;
    arg.s = s;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if ((ret = pthread_create(&thread, &attr, main_test_thread, &arg)) != 0) {
        fprintf(stderr, "Can't create main test thread: %s\n",
                strerror(ret));
        exit(1);
    }
    return thread;
}
