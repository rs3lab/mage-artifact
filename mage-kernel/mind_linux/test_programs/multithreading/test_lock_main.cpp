#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <pthread.h>
#include <sys/mman.h>
#include "lock_test.hpp"

void exit_gracefully(int gtid) {
    //printf("thread %d exit gracefully\n", tid);
    while (1);
}

int pin_to_core(int core_id) {
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (core_id < 0 || core_id >= num_cores) {
        printf("pin to core[%d] failed, total cores[%d]\n", core_id, num_cores);
        return -1;
    }
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_t current_thread = pthread_self();
    int err = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
    printf("main thread pin to core %d ret[%d]\n", core_id, err);
    usleep(10000);
    return err;
}

void f(void *arg) {
    trace_t trace = *(trace_t *)arg;
    //printf("hello from remote thread[%d]\n", trace.get_tid());
    //trace.pin_to_core(trace.get_tid()); shouldn't be necessary
    trace.wait_lock_test_start();
    trace.do_lock_test();
    *(trace_t *)arg = trace;
    trace.wait_lock_test_finish();
    exit_gracefully(trace.get_gtid());
}

int main (int argc, char *argv[]) {
    if (argc != 4) {
        printf("please enter number of remote blades, threads and res file location\n");
        return -1;
    }
    int num_remote_blades = atoi(argv[1]);
    int num_remote_threads_per_blade = atoi(argv[2]);
    int num_remote_threads_tot = num_remote_blades * num_remote_threads_per_blade;
    std::string res_dir = argv[3];
    printf("main starts\n");

    //pin to core
    pin_to_core(0);

    //create trace & load data
    //char *data_buf = new char[ALLOC_REGION_SIZE]{0};
    char *data_buf = (char *)mmap(NULL, ALLOC_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (!data_buf) {
        printf("fail to allocate data buf\n");
        return -1;
    } else printf("buf[%p]\n", data_buf);
    memset(data_buf, 0, ALLOC_REGION_SIZE);

    //test counter to verify lock implementation
    unsigned long *test_cnt = (unsigned long *)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (!data_buf) {
        printf("fail to allocate test_cnt\n");
        return -1;
    } else printf("test_cnt[%p]\n", test_cnt);
    *test_cnt = 0;

    trace_t *traces = create_trace_for_lock_test(num_remote_blades, num_remote_threads_per_blade, data_buf, test_cnt);
    if (!traces) {
        printf("fail to create traces for lock test\n");
        return -1;
    }

    //create remote threads
    pthread_t *remote_threads = new pthread_t[num_remote_threads_tot];
    if (!remote_threads) {
        printf("fail to alloc pthreads for remote threads\n");
        return -1;
    }
    for (int i = 0; i < num_remote_threads_tot; ++i) {
        //printf("args[%d] :%p\n", i, &traces[i]);
        pthread_create(&remote_threads[i], NULL,
            (void *(*)(void *))f, (void *)(&traces[i]));
        sleep(3);
    }
    printf("done launching all remote threads\n");

#ifdef USE_SPINLOCK
    start_lock_test((pthread_spinlock_t *)(data_buf + TEST_METADATA_SIZE), (int *)data_buf);
#elif defined USE_MUTEX
    start_lock_test((pthread_mutex_t *)(data_buf + TEST_METADATA_SIZE), (int *)data_buf);
#elif defined USE_RLOCK || defined USE_WLOCK || defined USE_RWLOCK
    start_lock_test((pthread_rwlock_t *)(data_buf + TEST_METADATA_SIZE), (int *)data_buf);
#else
    start_lock_test((mindlock_t *)(data_buf + TEST_METADATA_SIZE), (int *)data_buf);
#endif
    printf("coherence test started\n");

    finish_lock_test(((int *)data_buf) + 1, num_remote_threads_tot, traces, res_dir, test_cnt);
    printf("coherence test finished, mission complete for main thread\n");
    exit_gracefully(-1);
    //for (int i = 0; i < num_remote_threads; ++i) {
    //    pthread_join(tids[i], NULL);
    //}
    return 0;
}
