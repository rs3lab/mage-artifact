#ifndef __ROCE_DISAGG_MODULE_H__
#define __ROCE_DISAGG_MODULE_H__

// NOW __CN_ROCE__ and __CN_ROCE_TEST__ are included in Makefile

#include "../../include/disagg/header_for_ofa_kernel.h"

#include "../../include/disagg/network_disagg.h"
#include "../../include/disagg/cnthread_disagg.h"
#include "../../include/disagg/profile_points_disagg.h"

#include <linux/module.h>

#include <linux/kthread.h>
#include <linux/sched/signal.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/unistd.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/ktime.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>
#include <linux/inet.h>

#define MIND_RDMA_IB_DEVNAME "mlx5_0"

// Parameters for CN and FH.
#define MIND_RDMA_FHQP_TRANSMISSION_DEPTH 1
#define MIND_RDMA_FHQP_SIZE               MIND_RDMA_FHQP_TRANSMISSION_DEPTH
#define MIND_RDMA_FHCQ_SIZE               MIND_RDMA_FHQP_TRANSMISSION_DEPTH
#define MIND_RDMA_FHCQ_POLL_BATCH_SIZE    1

#define MIND_RDMA_CNQP_TRANSMISSION_DEPTH CNTHREAD_RECLAIM_BATCH_SIZE
#define MIND_RDMA_CNQP_SIZE               MIND_RDMA_CNQP_TRANSMISSION_DEPTH
#define MIND_RDMA_CNCQ_SIZE               NUM_CNTHREADS
#define MIND_RDMA_CNCQ_POLL_BATCH_SIZE    1

// Not yet split between FH and CN.
#define MIND_RDMA_POLL_CQS_TIMEOUT_MS        10000ULL
#define MIND_RDMA_POLL_CQS_BACKOFF_NS        2000ULL
#define MIND_RDMA_READ_SYNC_POLL_BACKOFF_NS  500ULL
#define MIND_RDMA_WRITE_SYNC_POLL_BACKOFF_NS 500ULL

#define MIND_RDMA_RMEM_SIZE_MIB              32768ULL
#define MIND_RDMA_CM_TIMEOUT_MS              10000ULL

// Upper bounds
#define MIND_RDMA_MAX_NUM_FHQP               128
#define MIND_RDMA_MAX_NUM_CNQP               128
#define MIND_RDMA_MAX_QP_TRANSMISSION_DEPTH (MIND_RDMA_CNQP_TRANSMISSION_DEPTH)

enum cm_type {
	MIND_RDMA_FH_CM = 0,
	MIND_RDMA_CN_CM = 1,
};

struct mind_rdma_cm_state {
	// Are we FH or CN?
	enum cm_type                 cm_type;
	// mind-specific identifier (index into relevant `cm` array).
	int                          cm_num;
	struct mind_rdma_state       *rdma_state;
	struct rdma_cm_id            *cm_id;
	struct completion            cm_done;
	int                          cm_error;
	struct ib_qp                 *qp; // ownership. free when done.
	struct ib_cq                 *cq; // no ownership; do not free when done!
	bool                         is_qp_used; // set if assigned to a thread.
	spinlock_t                   qp_lock; // guards cq + qp related fields
};

struct mind_rdma_state {
	struct ib_device             *dev;
	struct ib_pd                 *pd;
	// Fault Handler Connection Manager
	struct mind_rdma_cm_state    *fhcm[MIND_RDMA_MAX_NUM_FHQP];
	// CNThread Connection Manager
	struct mind_rdma_cm_state    *cncm[MIND_RDMA_MAX_NUM_CNQP];
	// Completion queues; multiplexed among connections
	struct ib_cq                 *fhcqs[MIND_RDMA_MAX_NUM_FHQP]; // ownership
	struct ib_cq                 *cncqs[MIND_RDMA_MAX_NUM_CNQP]; // ownership

	int                          num_fhqps;
	int                          num_cnqps;
	int                          num_fhcqs;
	int                          num_cncqs;

	// address and port: alive until the module is removed
	char                         *mem_server_ip;
	struct sockaddr_storage      server_addr;
 	__u32                        mem_server_port_start;
	// Every QP sets up its connection on a different port. This var tracks
	// the "current" (max) used port, so subsequent connections know which
	// port to use.
	__u32                        mem_server_port;
	__u64                        mem_server_base_addr;
	__u64                        mem_server_mem_size;
	__u32                        mem_server_rkey;

	struct completion            init_done;
};

int init_rdma_conn_to_mn(int num_cnqps, int num_fhqps);
int destroy_rdma_conn_to_mn(void);


// ----------------------------------
// y: MIND RoCE API functions
// ----------------------------------

// QP request API
void *mind_rdma_get_fhqp_handle(void);
void *mind_rdma_get_cnqp_handle(void);
int mind_rdma_put_qp_handle(void *qp_handle);

// Page Map/Unmap API
__u64 mind_rdma_map_dma(struct page *page, size_t size_bytes);
void mind_rdma_unmap_dma(u64 laddr_dma, size_t size_bytes);

// Sync API
int mind_rdma_read_sync(void *qp_handle, void *lbuf, u64 size, unsigned long remote_addr);
int mind_rdma_write_sync(void *qp_handle, void *lbuf, u64 size, unsigned long remote_addr);

// Batched API
void mind_rdma_initialize_batched_write(struct mind_rdma_reqs *out);
int mind_rdma_batched_write(void *qp_handle, struct mind_rdma_reqs *reqs, int req_offset);

// Async API
int mind_rdma_read(void *qp_handle, void *buf, unsigned long addr, unsigned long len,
		struct mind_rdma_req *out);
int mind_rdma_write(void *qp_handle, void *lbuf, unsigned long addr, unsigned long len,
		struct mind_rdma_req *out);
int mind_rdma_check_cq(void *qp_handle, struct mind_rdma_req *target);
int mind_rdma_check_cqs(void *qp_handle, int num_targets, struct ib_wc *wc_arr);
int mind_rdma_poll_cq(void *qp_handle, struct mind_rdma_req *target);
int mind_rdma_poll_cqs(void * qp_handle, struct mind_rdma_req *targets, int num_targets);

// ----------------------------------
// y: Profiling Points
// ----------------------------------

// DECLARE_PP(CN_loop);
// DECLARE_PP(CN_send);

// DECLARE_PP(CN_a);
// DECLARE_PP(CN_b);
// DECLARE_PP(CN_c);
// DECLARE_PP(CN_d);
// DECLARE_PP(CN_e);

// DECLARE_PP(FH_loop);
// DECLARE_PP(FH_send);

// DECLARE_PP(FH_a);
// DECLARE_PP(FH_b);
// DECLARE_PP(FH_c);
// DECLARE_PP(FH_d);
// DECLARE_PP(FH_e);


// DECLARE_PP(NET_dma_map);

// DECLARE_PP(NET_a);
// DECLARE_PP(NET_b);
// DECLARE_PP(NET_c);
// DECLARE_PP(NET_d);
// DECLARE_PP(NET_e);

#endif  /* __NETWORK_SERVER_MODULE_H__ */

/* vim: set sw=8 ts=8 noexpandtab */
