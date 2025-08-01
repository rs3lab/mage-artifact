#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <arpa/inet.h>
#include <stdatomic.h>

#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

struct mr_info
{
	uint64_t remote_addr; // y: not used when CN->MN.
	uint32_t rkey;        // y: not used when CN->MN; rdmacm does that separately
	uint64_t mem_size;    // y: During CN->MN, CN sets this field to request a MR size.
};

#define PAGE_SIZE (1 << 12) // 4 KB
#define MIND_DEFAULT_CONTROL_PORT 58675
#define TIMEOUT_IN_MS 5000
#define MIND_QUEUE_SIZE 128	// number of in-flight msgs

extern struct sockaddr_in addr;
extern struct rdma_event_channel *ec;
extern struct ibv_qp_init_attr qp_attr;
extern struct rdma_cm_id *conn;
extern struct ibv_pd *pd;
extern struct ibv_mr *mr;
extern struct ibv_cq *cq;
extern char *buffer;
extern uint64_t buffer_size;
extern atomic_uint *alloc_array;
extern struct rdma_cm_event *event;

enum rdma_cm_event_type check_cm_event(void);

void rdma_init(void);
void rdma_init_finish(void);
void rdma_deinit(void);

#endif
