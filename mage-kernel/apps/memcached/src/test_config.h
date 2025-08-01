#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

#define N_TEST_THREADS 24
//#define N_TEST_THREADS 12

#define MAX_TEST_THREADS 28

#define MAX_TEST_KEY_LEN 16

#define MAX_TEST_DATA_LEN 960

#define DEFAULT_BUFFER_SIZE 1024

//#define N_FILL_REQS 748982
#define N_FILL_REQS 1497964

#define N_TEST_REQS 1000000

#define TEST_WRITE_RATIO 2  /* 0.2% percent writes */

#define TEST_ZIPFIAN 0.99 /* zipfian alpha */

#define LATENCY_TEST 1 /* Whether latency is measured or not */

#define LATENCY_SAMPLE_GAP 4 /* Sampling */

#define NUMA_CPUS 28 /* 28 CPUs for 1 NUMA node */


#endif
