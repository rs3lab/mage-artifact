#ifndef TEST_THREAD_H
#define TEST_THREAD_H
#include "zipf/zipf.h"
#include "memcached.h"

struct barrier {
    pthread_cond_t complete;
    pthread_mutex_t mutex;
    int count;
    int crossing;
};

enum access_pattern {
    access_parallel,
    access_zipfian
};

struct main_test_arg {
    int n_test_threads;
    struct settings *s;
};

struct test_settings {
    int n_test_threads;
    int tid;
    int write_ratio; /* Out of 1000 */
    int n_requests; /* The number of requests for each thread */
    int key_size;
    int data_size;
    int test_port;
    enum access_pattern pattern;
    double zipfian;  /* The zipfian value for access_zipfian */

    struct barrier *b; /* Pointer to the barrier */
    unsigned short seed[3]; /* Seed for generating the op */
    uint8_t item_lock_type; /* use fine-grained or global item lock */
    struct zipf_state zs; /* The zipfian state used for key */
} __attribute__ ((aligned(64)));

#endif