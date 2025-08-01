#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "mind_sync_util.hpp"

//#define PAGE_SIZE 4096UL
#define TEST_METADATA_SIZE (PAGE_SIZE << 8)
#define TEST_REGION_SIZE (PAGE_SIZE << 8)
#define ALLOC_REGION_SIZE (TEST_REGION_SIZE + TEST_METADATA_SIZE)
#define TRACE_LEN 100000
//#define MAX_LOCK_CNT 10000
#define MAX_LOCK_CNT 1000
#define CDF_BUCKET_NUM 512

//#define USE_SPINLOCK
//#define USE_MUTEX
//#define USE_RLOCK
//#define USE_WLOCK
#define USE_RWLOCK

//#define HOLD_LOCK_US 1000
//#define RETRY_LOCK_US 500
#define HOLD_LOCK_US 50000
#define RETRY_LOCK_US 25000
//#define VERIFY_LOCK
class trace_t {
private:
    int nid;
    int tid;
    int gtid;
    int num_remote_threads_tot;
#ifdef USE_SPINLOCK
    pthread_spinlock_t *mindlock;
#elif defined USE_MUTEX
    pthread_mutex_t *mindlock;
#elif defined USE_RLOCK || defined USE_WLOCK || defined USE_RWLOCK
    pthread_rwlock_t *mindlock;
#else
	mindlock_t *mindlock;
#endif
    unsigned long *test_cnt;
	char *meta_buf;
    unsigned long lock_cdf[CDF_BUCKET_NUM];
    unsigned long ulock_cdf[CDF_BUCKET_NUM];
    unsigned long lulock_cdf[CDF_BUCKET_NUM];
    unsigned long lock_tot_time;
    unsigned long ulock_tot_time;
    unsigned long tot_time;
	//int node_idx;
	//int num_nodes;
	//int master_thread;
public:
    trace_t(int nid, int tid, int gtid, char *_buf, int _num_remote_threads, unsigned long *test_cnt);
    int pin_to_core(int core_id);
    inline int get_gtid() {return gtid;}
    inline int get_num_remote_threads() {return num_remote_threads_tot;}
    void wait_lock_test_start(void);
    void wait_lock_test_finish(void);
    void do_lock_test(void);
    unsigned long get_lock_cdf_at(int idx) {return lock_cdf[idx];}
    unsigned long get_ulock_cdf_at(int idx) {return ulock_cdf[idx];}
    unsigned long get_lulock_cdf_at(int idx) {return lulock_cdf[idx];}
    unsigned long get_tot_time() {return tot_time;}
    void print_lock_test_result(std::string res_dir);
};

trace_t *create_trace_for_lock_test(int num_blades, int num_remote_threads_per_blade, char *data_buf, unsigned long *test_cnt);

#ifdef USE_SPINLOCK
void start_lock_test(pthread_spinlock_t *mindlock, int *sync_buf);
#elif defined USE_MUTEX
void start_lock_test(pthread_mutex_t *mindlock, int *sync_buf);
#elif defined USE_RLOCK || defined USE_WLOCK || defined USE_RWLOCK
void start_lock_test(pthread_rwlock_t *mindlock, int *sync_buf);
#else
void start_lock_test(mindlock_t *mindlock, int *sync_buf);
#endif

void finish_lock_test(int *sync_buf, int num_remote_threads, trace_t *traces, std::string res_dir, unsigned long *test_cnt);
void print_lock_test_result(trace_t *traces, int num_remote_threads_tot, std::string res_dir, unsigned long *test_cnt);