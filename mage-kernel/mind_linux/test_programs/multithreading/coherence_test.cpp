#include <pthread.h>
#include <unistd.h>
#include "coherence_test.hpp"

//#define SLEEP
//#define PRINT

unsigned long my_sleep(unsigned long usec) {
#ifdef SLEEP
    usleep(usec);
#else
    unsigned long n = usec * 500;
    for (unsigned long i = 0; i < n; ++i);
#endif
    return 0;
}

trace_t::trace_t(unsigned long _len, char *_buf, int _num_remote_threads) {
    op = new char[_len];
	addr = new unsigned long[_len];
	val = new char[_len];
    printf("op[%p - %p] addr[%p - %p] val[%p - %p]\n",
        op, op + _len, addr, addr + _len, val, val + _len);
    len = _len;
    meta_buf = _buf;
    data_buf = _buf + TEST_METADATA_SIZE;
    num_remote_threads = _num_remote_threads;
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

inline void trace_t::set_trace_at(FILE *fp, unsigned long j) {
    fscanf(fp, "%c %lu %hhu\n", &op[j], &addr[j], &val[j]);
}

void start_coherence_test(int *sync_buf) {
    *sync_buf = -1;
}

void trace_t::wait_coherence_test_start(void) {
    int *sync_buf = (int *)meta_buf;
    unsigned long cnt = 0;
    while (*sync_buf != -1) {
#ifdef PRINT
        if (cnt % 2000 == 0)
            printf("wait start tid[%d] cnt[%d] sync_val[%d]\n", tid, cnt, *sync_buf);
#endif
        ++cnt;
        my_sleep(500);
    }
}

void finish_coherence_test(int *sync_buf, int num_remote_threads,
    trace_t *traces, std::string res_dir) {
    unsigned long cnt = 0;
    //wait for all threads done coherence test
#ifdef PRINT
    printf("sync_buf[%p] *sync_buf[%d]\n", sync_buf, *sync_buf);
#endif
    while (*sync_buf != num_remote_threads) {
#ifdef PRINT
        if (cnt % 2000 == 0)
            printf("wait all remote threads cnt[%d] sync_val[%d]\n", cnt, *sync_buf);
#endif
        ++cnt;
        my_sleep(1000);
    }
    for (int i = 0; i < num_remote_threads; ++i)
        traces[i].print_coherence_test_result(res_dir);
    *sync_buf = -1;
}

void trace_t::wait_coherence_test_finish(void) {
    unsigned long cnt = 0;
    int *sync_buf = ((int *)meta_buf) + 1;
    //notice main thread all remote threads done
#ifdef PRINT
    printf("sync_buf[%p] *sync_buf[%d] tid[%d]\n", sync_buf, *sync_buf, tid);
#endif
    while (*sync_buf != tid) {
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

void trace_t::do_coherence_test(void) {
    for (int i = 0; i < len; ++i) {
        char _op = op[i];
		if (_op == 'r') {
			val[i] = data_buf[addr[i]];
		} else if(_op == 'w') {
			data_buf[addr[i]] = val[i];
		} else {
            printf("unexpected op %c\n", _op);
		}
#ifdef PRINT
		if (i % 1000 == 0)
			printf("%d\n", i);
#endif
		if (i % 20 == 0)
            my_sleep(200);
	}
}

void trace_t::print_coherence_test_result(std::string res_dir) {
    std::string res_name = res_dir + std::string("/") + std::to_string(tid);
    FILE *fp = fopen(res_name.c_str(), "w");
    for (int i = 0; i < len; ++i) {
	    if (!fp) {
            printf("fail to open res file %s\n", res_name.c_str());
		    return;
	    }
        fprintf(fp, "%hhu\n", val[i]);
    }
    fclose(fp);
}

trace_t *create_trace_for_coherence_test(int num_remote_threads, std::string trace_dir, char *data_buf) {
    trace_t *traces = (trace_t *)malloc(sizeof(trace_t) * num_remote_threads);
    if (!traces) {
        printf("fail to allocate traces\n");
        return NULL;
    } else printf("traces[%p] allocated\n", traces);
    for (int i = 0; i < num_remote_threads; ++i) {
        traces[i] = trace_t(TRACE_LEN, data_buf, num_remote_threads);
        std::string trace_name = trace_dir + std::string("/") + std::to_string(i);
        FILE *fp = fopen(trace_name.c_str(), "r");
	    if (!fp) {
            printf("fail to open trace file %s\n", trace_name.c_str());
		    return NULL;
	    }
        traces[i].set_tid(i);
        for (int j = 0; j < TRACE_LEN; ++j)
            traces[i].set_trace_at(fp, j);
        fclose(fp);
    }
    return traces;
}