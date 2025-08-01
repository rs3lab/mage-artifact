#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libmemcached/memcached.h>
#include <math.h>
#include <unistd.h>
#include <x86intrin.h>          // __rdtscp
#include <time.h>
#include <inttypes.h>
#include <stdatomic.h>

// Around 4 GiB.
#define NUM_KEYS 4194304
#define ZIPF_SKEW 0.99
#define MAX_KEY_SIZE 32
#define VAL_SIZE 1024
#define NUM_THREADS 24

char *zipf_data;
// Zipfian distribution state
double *zipf_cdf;

// Argv
const char *server;
int port;
int total_iterations;

static double  cycles_per_ns;          /* filled once in main()           */
static uint64_t pause_cycles = 0;      /* Δ‑TSC to wait inside the loop   */


typedef struct {
    int         id;
    uint32_t   *lat;     /* filled by the thread */
    int         count;   /* iterations actually executed */
} thread_args_t;


static inline uint64_t rdtscp(void)
{
    unsigned aux;
    return __rdtscp(&aux);      // serialising read
}

static inline void busy_pause(void)
{
    if (pause_cycles == 0) return;
    uint64_t target = rdtscp() + pause_cycles;
    while (rdtscp() < target)
        _mm_pause();                   /* polite to the core             */
}

/* ------------------------------------------------------------------ */
/* Measure cycles per nanosecond once at start‑up (±0.1 %)            */
static double measure_cycles_per_ns(void)
{
    struct timespec ts_start, ts_end;
    uint64_t c_start, c_end;

    clock_gettime(CLOCK_MONOTONIC_RAW, &ts_start);
    c_start = rdtscp();

    /* 100 ms sleep gives a large enough Δ   */
    struct timespec req = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
    nanosleep(&req, NULL);

    clock_gettime(CLOCK_MONOTONIC_RAW, &ts_end);
    c_end = rdtscp();

    double ns = (double)(ts_end.tv_sec  - ts_start.tv_sec ) * 1e9 +
                (double)(ts_end.tv_nsec - ts_start.tv_nsec);
    return (c_end - c_start) / ns;           /* cycles / ns  */
}




static void generate_zipf_cdf(double skew, int n) {
    zipf_cdf = (double *)malloc(n * sizeof(double));
    double sum = 0.0;
    for (int i = 1; i <= n; i++) {
        sum += 1.0 / pow(i, skew);
    }
    double cumulative_sum = 0.0;
    for (int i = 0; i < n; i++) {
        cumulative_sum += 1.0 / pow(i + 1, skew);
        zipf_cdf[i] = cumulative_sum / sum;
    }
}

static void generate_zipf_data(void) {
    size_t data_size_bytes = (VAL_SIZE + NUM_KEYS) * sizeof(*zipf_data);

    zipf_data = malloc(data_size_bytes);

    int fd = open("/dev/urandom", O_RDONLY);
    read(fd, zipf_data, data_size_bytes);
    close(fd);
}

static int zipf_sample(int n) {
    double r = (double) rand() / RAND_MAX;

    int left = 0;
    int right = NUM_KEYS - 1;
    int highest_below_r = 0;  // Store the closest value strictly below the key

    while (left <= right) {
        int mid = (right + left) / 2;
        // printf("%d, %d, %d\n", left, mid, right);

        if (zipf_cdf[mid] < r) {
            highest_below_r = mid;  // Update best candidate
            left = mid + 1;  // Search the right half
        } else {
            right = mid - 1;  // Search the left half
        }
    }

    return highest_below_r;
}

void *memcached_load_generator(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    char key[MAX_KEY_SIZE];
    int thread_iterations;
    int counter = 0;

    thread_iterations = total_iterations / NUM_THREADS;
    if (args->id == 0)
        thread_iterations += total_iterations % NUM_THREADS;

    args->lat   = malloc(thread_iterations * sizeof(uint32_t));
    args->count = thread_iterations;

    // Initialize memcached client
    memcached_st *memc;
    memcached_server_st *servers;
    memcached_return rc;
    memc = memcached_create(NULL);
    servers = memcached_server_list_append(NULL, server, port, &rc);
    memcached_server_push(memc, servers);

    for (int i = 0; i < thread_iterations; i++) {
        int key_index = zipf_sample(NUM_KEYS);
        snprintf(key, MAX_KEY_SIZE, "key_%d", key_index);
        // printf("%s\n", key);

        // 2% write
        bool write = false;
        if (counter == 0 || counter == 50)
             write = true;

        if (counter <= 0)
             counter = 100;
        counter--;

        uint64_t t0 = rdtscp();

        if (!write) {  // `get`
            memcached_return rc;
            size_t value_length;
            uint32_t flags;
            char *retrieved_value = memcached_get(memc, key, strlen(key), &value_length, &flags, &rc);

            uint64_t t1 = rdtscp();
            args->lat[i] = (uint32_t)(t1 - t0);

            if (rc == MEMCACHED_SUCCESS) {
                free(retrieved_value);
            }
        } else {  // `set`
            memcached_return rc;

            rc = memcached_set(memc, key, strlen(key), &zipf_data[key_index], VAL_SIZE,
                    (time_t)0, (uint32_t)0);

            uint64_t t1 = rdtscp();
            args->lat[i] = (uint32_t)(t1 - t0);

            if (rc != MEMCACHED_SUCCESS) {
                printf("ERROR: %s\n", memcached_strerror(memc, rc));
                exit(1);
            }
        }

        busy_pause();
    }

    memcached_free(memc);
    memcached_server_list_free(servers);
    pthread_exit(NULL);
}

static int cmp_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a;
    uint32_t y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server> <port> <total_iterations> <pause_us>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct timespec bench_start, bench_end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &bench_start);   /* T0 */

    server = argv[1];
    port = atoi(argv[2]);
    total_iterations = atoi(argv[3]);
    int pause_us = atoi(argv[4]);

    double cycles_per_ns = measure_cycles_per_ns();
    pause_cycles  = (uint64_t)(cycles_per_ns * 1000.0 * pause_us);

    // Prepare Zipfian CDF for key distribution
    printf("Init\n");
    fflush(stdout);
    generate_zipf_cdf(ZIPF_SKEW, NUM_KEYS);
    generate_zipf_data();

    // Create threads
    pthread_t threads[NUM_THREADS];
    thread_args_t thread_args[NUM_THREADS];

    printf("BEGIN_BENCHMARK\n"); // signal to test framework
    fflush(stdout);
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].id = i;
        pthread_create(&threads[i], NULL, memcached_load_generator, (void *)&thread_args[i]);
    }

    // Wait for threads to finish
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &bench_end);     /* T1 */


    // Cleanup
    free(zipf_cdf);
    free(zipf_data);

    // CALCULATE P99 IN US

    size_t total = 0;
    for (int i = 0; i < NUM_THREADS; ++i)
         total += thread_args[i].count;

    uint32_t *all = malloc(total * sizeof(uint32_t));
    size_t off = 0;
    for (int i = 0; i < NUM_THREADS; ++i) {
        memcpy(all + off, thread_args[i].lat,
                thread_args[i].count * sizeof(uint32_t));
        off += thread_args[i].count;
        free(thread_args[i].lat);
    }

    /* one global sort */
    qsort(all, total, sizeof(uint32_t), cmp_u32);   /* 32‑bit compare */

    size_t idx99 = (size_t)(0.99 * total);
    uint32_t p99_cycles = all[idx99];
    double p99_us = (double)p99_cycles / cycles_per_ns / 1000.0;

    printf("p99 latency: %.2f µs (%" PRIu32 " cycles)\n",
            p99_us, p99_cycles);

    /* total operations executed = sum of per‑thread 'count' */
    double elapsed_ns = (double)(bench_end.tv_sec  - bench_start.tv_sec) * 1e9 +
                        (double)(bench_end.tv_nsec - bench_start.tv_nsec);
    double elapsed_s  = elapsed_ns * 1e-9;
    double ops       = (double)total / elapsed_s;
    printf("throughput: %.2f ops/s)\n", ops);

    free(all);

    printf("END_BENCHMARK\n"); // signal to test framework
    fflush(stdout);


    return EXIT_SUCCESS;
}

