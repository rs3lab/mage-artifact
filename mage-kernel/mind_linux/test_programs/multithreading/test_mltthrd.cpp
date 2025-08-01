#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <pthread.h>
#include <sys/mman.h>
#include "coherence_test.hpp"

void exit_gracefully(int tid) {
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
    trace.pin_to_core(trace.get_tid());
    trace.wait_coherence_test_start();
    trace.do_coherence_test();
    trace.wait_coherence_test_finish();
    exit_gracefully(trace.get_tid());
}

int main (int argc, char *argv[]) {
    if (argc != 4) {
        printf("please enter number of remote threads, trace file location and res file location\n");
        return -1;
    }
    int num_remote_threads = atoi(argv[1]);
    std::string trace_dir = argv[2];
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
    trace_t *traces = create_trace_for_coherence_test(num_remote_threads, trace_dir, data_buf);
    if (!traces) {
        printf("fail to create traces for coherence test\n");
        return -1;
    }

    //create remote threads
    pthread_t *remote_threads = new pthread_t[num_remote_threads];
    if (!remote_threads) {
        printf("fail to alloc pthreads for remote threads\n");
        return -1;
    }
    for (int i = 0; i < num_remote_threads; ++i) {
        //printf("args[%d] :%p\n", i, &traces[i]);
        pthread_create(&remote_threads[i], NULL,
            (void *(*)(void *))f, (void *)(&traces[i]));
        sleep(5);
    }
    printf("done launching all remote threads\n");

    
    start_coherence_test((int *)data_buf);
    printf("coherence test started\n");

    finish_coherence_test(((int *)data_buf) + 1, num_remote_threads, traces, res_dir);
    printf("coherence test finished, mission complete for main thread\n");
    exit_gracefully(-1);
    //for (int i = 0; i < num_remote_threads; ++i) {
    //    pthread_join(tids[i], NULL);
    //}
    return 0;
}
