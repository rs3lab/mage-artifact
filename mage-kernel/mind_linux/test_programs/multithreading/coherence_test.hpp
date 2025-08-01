#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>

#define PAGE_SIZE 4096UL
#define TEST_METADATA_SIZE (PAGE_SIZE << 8)
#define TEST_REGION_SIZE (PAGE_SIZE << 8)
#define ALLOC_REGION_SIZE (TEST_REGION_SIZE + TEST_METADATA_SIZE)
#define TRACE_LEN 100000

class trace_t {
private:
    int tid;
    int num_remote_threads;
	char *op;
	unsigned long *addr;
	char *val;
	unsigned long len;
	char *meta_buf;
	char *data_buf;
	//int node_idx;
	//int num_nodes;
	//int master_thread;
public:
    trace_t(unsigned long _len, char *_buf, int _num_remote_threads);
    int pin_to_core(int core_id);
    inline int get_tid() {return tid;}
    inline void set_tid(int _tid) {tid = _tid;}
    inline int get_num_remote_threads() {return num_remote_threads;}
    inline void set_trace_at(FILE *fp, unsigned long j);
    void wait_coherence_test_start(void);
    void wait_coherence_test_finish(void);
    void do_coherence_test(void);
    void print_coherence_test_result(std::string res_dir);
};

trace_t *create_trace_for_coherence_test(int num_remote_threads,
    std::string trace_dir, char *data_buf);
void start_coherence_test(int *sync_buf);
void finish_coherence_test(int *sync_buf, int num_remote_threads,
    trace_t *traces, std::string res_dir);