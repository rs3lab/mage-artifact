#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <chrono>
#include "lock_test.hpp"

//#define SLEEP
//#define PRINT
#define PRINT_PROGRESS

unsigned long my_sleep(unsigned long usec) {
#ifdef SLEEP
    usleep(usec);
#else
    unsigned long n = usec * 500;
    for (unsigned long i = 0; i < n; ++i);
#endif
    return 0;
}


trace_t::trace_t(int _nid, int _tid, int _gtid, char *_buf, int _num_remote_threads_tot, unsigned long *_test_cnt) {
    meta_buf = _buf;
#ifdef USE_SPINLOCK
    mindlock = (pthread_spinlock_t *)(_buf + TEST_METADATA_SIZE);
#elif defined USE_MUTEX
    mindlock = (pthread_mutex_t *)(_buf + TEST_METADATA_SIZE);
#elif defined USE_RLOCK || defined USE_WLOCK || defined USE_RWLOCK
    mindlock = (pthread_rwlock_t *)(_buf + TEST_METADATA_SIZE);
#else
    mindlock = (mindlock_t *)(_buf + TEST_METADATA_SIZE);
#endif
    test_cnt = _test_cnt;
    nid = _nid;
    tid = _tid;
    gtid = _gtid;
    num_remote_threads_tot = _num_remote_threads_tot;
    memset(lock_cdf, 0, sizeof(lock_cdf));
    memset(ulock_cdf, 0, sizeof(ulock_cdf));
    memset(lulock_cdf, 0, sizeof(lock_cdf));
    lock_tot_time = 0;
    ulock_tot_time = 0;
    tot_time = 0;
    printf("n[%d] t[%d] gt[%d] meta_buf[%p] mindlock[%p]\n", nid, tid, gtid, meta_buf, mindlock);
}

int trace_t::pin_to_core(int core_id) {
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (core_id < 0 || core_id >= num_cores) {
#ifdef PRINT
        printf("pin to core[%d] failed, total cores[%d]\n", core_id, num_cores);
#endif
        return -1;
    }
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_t current_thread = pthread_self();
    int err = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
#ifdef PRINT
    printf("thread %d pin to core %d ret[%d]\n", tid, core_id, err);
#endif
    my_sleep(10000);
    return err;
}


#ifdef USE_SPINLOCK
void start_lock_test(pthread_spinlock_t *mindlock, int *sync_buf) {
    pthread_spin_init(mindlock, PTHREAD_PROCESS_PRIVATE);
    *sync_buf = -1;
}
#elif defined USE_MUTEX
void start_lock_test(pthread_mutex_t *mindlock, int *sync_buf) {
    pthread_mutex_init(mindlock, NULL);
    *sync_buf = -1;
}
#elif defined USE_RLOCK || defined USE_WLOCK || defined USE_RWLOCK
void start_lock_test(pthread_rwlock_t *mindlock, int *sync_buf) {
    pthread_rwlock_init(mindlock, NULL);
    *sync_buf = -1;
}
#else
void start_lock_test(mindlock_t *mindlock, int *sync_buf) {
    initMindLock(mindlock);
    *sync_buf = -1;
}
#endif


void trace_t::wait_lock_test_start(void) {
    int *sync_buf = (int *)meta_buf;
    unsigned long cnt = 0;
    while (*sync_buf != -1) {
#ifdef PRINT
        if (cnt % 2000 == 0)
            printf("wait start tid[%d] cnt[%d] sync_val[%d]\n", tid, cnt, *sync_buf);
#endif
        ++cnt;
        //no sleep, too slow
        //my_sleep(500);
    }
}


void finish_lock_test(int *sync_buf, int num_remote_threads_tot,
    trace_t *traces, std::string res_dir, unsigned long *test_cnt) {
    unsigned long cnt = 0;
    //wait for all threads done coherence test
#ifdef PRINT
    printf("sync_buf[%p] *sync_buf[%d]\n", sync_buf, *sync_buf);
#endif
    while (*sync_buf != num_remote_threads_tot) {
#ifdef PRINT
        if (cnt % 2000 == 0)
            printf("wait all remote threads cnt[%d] sync_val[%d]\n", cnt, *sync_buf);
#endif
        ++cnt;
        my_sleep(1000);
    }
    for (int i = 0; i < num_remote_threads_tot; ++i)
        traces[i].print_lock_test_result(res_dir);
    print_lock_test_result(traces, num_remote_threads_tot, res_dir, test_cnt);
    *sync_buf = -1;
}


void trace_t::wait_lock_test_finish(void) {
    unsigned long cnt = 0;
    int *sync_buf = ((int *)meta_buf) + 1;
    //notice main thread all remote threads done
#ifdef PRINT
    printf("sync_buf[%p] *sync_buf[%d] tid[%d]\n", sync_buf, *sync_buf, tid);
#endif
    while (*sync_buf != gtid) {
#ifdef PRINT
        if (cnt % 2000 == 0)
            printf("wait finish tid[%d] cnt[%d] sync_val[%d]\n", tid, cnt, *sync_buf);
#endif
        ++cnt;
        my_sleep(1000);
    }
    ++(*sync_buf);
    //wait for main thread print result
    while (*sync_buf != -1)
        my_sleep(1000);

}


static int latency_to_bkt(unsigned long lat_in_us)
{
    if (lat_in_us < 100)
        return (int)lat_in_us;
    else if (lat_in_us < 1000)
        return 100 + ((lat_in_us - 100) / 10);
    else if (lat_in_us < 10000)
        return 190 + ((lat_in_us - 1000) / 100);
    else if (lat_in_us < 100000)
        return 280 + ((lat_in_us - 10000) / 1000);
    else if (lat_in_us < 1000000)
        return 370 + ((lat_in_us - 100000) / 10000);
    return CDF_BUCKET_NUM - 1;    // over 1 sec
}


void trace_t::do_lock_test(void) {
    auto t_start = std::chrono::high_resolution_clock::now();
    unsigned long sleep_div_factor = 1;
    unsigned long max_lock_cnt = MAX_LOCK_CNT;

//special config for wrlock
#ifdef USE_RWLOCK
    if ((gtid & 1) != 0) {
        max_lock_cnt /= (num_remote_threads_tot / 2);
    } else {
        sleep_div_factor = 2;
    }
#endif

    for (unsigned long lock_cnt = 0; lock_cnt < max_lock_cnt; ++lock_cnt) {
        //auto t_start_lock = std::chrono::high_resolution_clock::now();
#ifdef USE_SPINLOCK
        pthread_spin_lock(mindlock);
#elif defined USE_MUTEX
        pthread_mutex_lock(mindlock);
#elif defined USE_RLOCK
        pthread_rwlock_rdlock(mindlock);
#elif defined USE_WLOCK
        pthread_rwlock_wrlock(mindlock);
#elif defined USE_RWLOCK
        if ((gtid & 1) == 0)
            pthread_rwlock_rdlock(mindlock);
        else
            pthread_rwlock_wrlock(mindlock);
#else
        while (!tryMindLock(mindlock, nid, tid)) {;}
#endif
        //auto t_end_lock = std::chrono::high_resolution_clock::now();
        
#ifdef VERIFY_LOCK
        ++(*test_cnt);
#endif

#ifdef HOLD_LOCK_US
        usleep(HOLD_LOCK_US / sleep_div_factor);
#endif

        //auto t_start_ulock = std::chrono::high_resolution_clock::now();
#ifdef USE_SPINLOCK
        pthread_spin_unlock(mindlock);
#elif defined USE_MUTEX
        pthread_mutex_unlock(mindlock);
#elif defined USE_RLOCK || defined USE_WLOCK || defined USE_RWLOCK
        pthread_rwlock_unlock(mindlock);
#else
        tryMindUnlock(mindlock, nid, tid);
#endif
        //auto t_end_ulock = std::chrono::high_resolution_clock::now();

        //std::chrono::duration<double, std::micro> t_double = t_end_lock - t_start_lock;
        //unsigned long lock_time = t_double.count();
        //++lock_cdf[latency_to_bkt(lock_time)];
        //lock_tot_time += lock_time;

        //t_double = t_end_ulock - t_start_ulock;
        //unsigned long ulock_time = t_double.count();
        //++ulock_cdf[latency_to_bkt(ulock_time)];
        //ulock_tot_time += ulock_time;

        //++lulock_cdf[latency_to_bkt(lock_time + ulock_time)];
        //tot_time += (lock_time + ulock_time);
        //if (lock_cnt % 10000 == 0)
        //    printf("%lu %lu\n", lock_cnt, t_double.count());
#ifdef RETRY_LOCK_US
        usleep(RETRY_LOCK_US);
#endif
#ifdef PRINT_PROGRESS
        if (lock_cnt % (max_lock_cnt / 100) == 0)
            printf("%lu\n", lock_cnt);
#endif

    }
    auto t_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> _t_double = t_end - t_start;
    tot_time = _t_double.count();
}

void trace_t::print_lock_test_result(std::string res_dir) {
    ;
}

void print_lock_test_result(trace_t *traces, int num_remote_threads_tot, std::string res_dir, unsigned long *test_cnt) {
    unsigned long sum;
    std::string res_name = res_dir + std::string("/res");
    FILE *fp = fopen(res_name.c_str(), "w");

    sum = 0;
    for (int i = 0; i < num_remote_threads_tot; ++i) {
        sum = std::max(sum, traces[i].get_tot_time());
    }
    fprintf(fp, "tot time: %lu\n", sum);
    fprintf(fp, "throughput: %lf lock-unlock/s\n", (((double)num_remote_threads_tot) * MAX_LOCK_CNT * 1000000) / sum);

    fprintf(fp, "lock cdf\n");
    for (int i = 0; i < CDF_BUCKET_NUM; ++i) {
        sum = 0;
        for (int j = 0; j < num_remote_threads_tot; ++j) {
            sum += traces[j].get_lock_cdf_at(i);
        }
        fprintf(fp, "\t%d\t%lu\n", i, sum);
    }
    fprintf(fp, "\n\nulock cdf\n");
    for (int i = 0; i < CDF_BUCKET_NUM; ++i) {
        sum = 0;
        for (int j = 0; j < num_remote_threads_tot; ++j) {
            sum += traces[j].get_ulock_cdf_at(i);
        }
        fprintf(fp, "\t%d\t%lu\n", i, sum);
    }
    fprintf(fp, "\n\nlulock cdf\n");
    for (int i = 0; i < CDF_BUCKET_NUM; ++i) {
        sum = 0;
        for (int j = 0; j < num_remote_threads_tot; ++j) {
            sum += traces[j].get_lulock_cdf_at(i);
        }
        fprintf(fp, "\t%d\t%lu\n", i, sum);
    }

    fclose(fp);

    printf("total increment: %ld\n", num_remote_threads_tot * MAX_LOCK_CNT);
    printf("test counter value: %ld\n", *test_cnt);
}


trace_t *create_trace_for_lock_test(int num_remote_blades, int num_remote_threads_per_blade, char *data_buf, unsigned long *test_cnt) {
    //use mmap to separate traces into different pages
    int num_remote_threads_tot = num_remote_blades * num_remote_threads_per_blade;
    trace_t *traces = (trace_t *)malloc(sizeof(trace_t) * num_remote_threads_tot);
    int i = 0;
    for (int n = 0; n < num_remote_blades; ++n) {
        for (int t = 0; t < num_remote_threads_per_blade; ++t) {
            traces[i] = trace_t(n, t, i, data_buf, num_remote_threads_tot, test_cnt);
            //printf("trace[%d][%d] at %p\n", n, t, traces[i]);
            ++i;
        }
    }
    return traces;
}