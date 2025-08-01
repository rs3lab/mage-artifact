/*
 * Disaggregated network stack on RDMA
 *
 * Initial version of this RoCE library was written 
 * based on LegoOS' and FIT library.
 * LegoOS: https://github.com/WukLab/LegoOS
 * LITE (base of FIT): https://github.com/WukLab/lite
 * 
 * However, to make it compatible with our RoCE virtualization layer 
 * on programmable switch, we ended up with rewriting most part of the code.
 * Hence, there only some similar namings now.
 */

#ifndef __NETWORK_FIT_DISAGGREGATION_H__
#define __NETWORK_FIT_DISAGGREGATION_H__

// Since this file also used in the switch to share the configuration
#ifndef NOT_IMPORT_LINUX

#ifndef BF_CONTROLLER
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/socket.h>
#include <disagg/cnthread_disagg.h>
#endif

#ifndef __CONTROLLER__
#ifndef __MEMORY_NODE__
// only for computing node
#include <rdma/ib_verbs.h>
#endif // __MEMORY_NODE__
#endif // __CONTROLLER__
#endif // NOT_IMPORT_LINUX


#ifdef __CONTROLLER__
// y: keep this in sync with `cnthread_disagg.h`!
#define DISAGG_NUM_CORES 54
#endif

// TODO: update these macros with the new correct numbers of QP, etc.

// #define MAX_NODE	CONFIG_FIT_NR_NODES
#define DISAGG_MAX_NODE 2	// only 2 (itself + controller)
#define DISAGG_MAX_COMPUTE_NODE 16 // maximum number of computing nodes (i.e., blades)
#define DISAGG_QP_NUM_INVAL_BUF 16
#define DISAGG_QP_NUM_INVAL_DATA 4	// helper CPU + 2 (invalidation and evict handler)
#define DISAGG_QP_NUM_DUMMY_NACK DISAGG_NUM_CORES
#define DISAGG_QP_PER_COMPUTE ((2 * DISAGG_NUM_CORES) + DISAGG_QP_NUM_INVAL_BUF + DISAGG_QP_NUM_INVAL_DATA + DISAGG_QP_NUM_DUMMY_NACK)

// QP
// - Page faults: 0 to DISAGG_NUM_CORES - 1
// - Page fault ACK/NACKs: DISAGG_NUM_CORES to 2 * DISAGG_NUM_CORES - 1
#define DISAGG_QP_ACK_OFFSET DISAGG_NUM_CORES
// - Invalidation: 2 * DISAGG_NUM_CORES
//				   to 3 * DISAGG_NUM_CORES - 1
#define DISAGG_QP_INV_ACK_OFFSET_START (DISAGG_QP_ACK_OFFSET + DISAGG_NUM_CORES)
#define DISAGG_QP_INV_ACK_OFFSET (DISAGG_QP_INV_ACK_OFFSET_START + DISAGG_QP_NUM_INVAL_BUF - 1)
// - Eviction (data flush): 2 * DISAGG_NUM_CORES + DISAGG_QP_NUM_INVAL_BUF
//							to + DISAGG_QP_NUM_INVAL_DATA
#define DISAGG_QP_EVICT_OFFSET_START (DISAGG_QP_INV_ACK_OFFSET + 1)
#define DISAGG_QP_EVICT_OFFSET_END (DISAGG_QP_EVICT_OFFSET_START + DISAGG_QP_NUM_INVAL_DATA - 1)
// - Dummy (to store incomplete RDMA data):
//		2 * DISAGG_NUM_CORES + DISAGG_QP_NUM_INVAL_BUF + DISAGG_QP_NUM_INVAL_DATA
//		to + DISAGG_QP_NUM_DUMMY_NACK
#define DISAGG_QP_DUMMY_OFFSET_START (DISAGG_QP_EVICT_OFFSET_END + 1)
#define DISAGG_QP_DUMMY_OFFSET_END (DISAGG_QP_DUMMY_OFFSET_START + DISAGG_QP_NUM_DUMMY_NACK - 1)
#define MAX_NODE_BIT 5	// 2^5 = 32 nodes
#define MIND_USE_PHY_DMA_TO_ROCE

#ifdef __CN_ROCE__	// only inside CPU blade modules
#define NUM_PARALLEL_CONNECTION 	(DISAGG_QP_PER_COMPUTE)
#else
#define NUM_PARALLEL_CONNECTION 	(DISAGG_MAX_COMPUTE_NODE * DISAGG_QP_PER_COMPUTE)
#endif

// Variables related to recv depth
#define RECV_DEPTH					(256)    // was 256
#define GET_POST_RECEIVE_DEPTH_FROM_POST_RECEIVE_ID(id) (id&0x000000ff)
#define DISAGG_ACK_BUF_SIZE (1024)	// 1 page
#define DISAGG_NACK_BUF_SIZE (4096)

#define LID_SEND_RECV_FORMAT "0000:0000:000000:000000:00000000000000000000000000000000"
#define MAX_CONNECTION DISAGG_MAX_NODE * NUM_PARALLEL_CONNECTION //Assume that MAX_CONNECTION is smaller than 256

#define ROCE_PORT 4791
#define MAX_PCIE_PAYLOAD	  256
#define MAX_INLINED_DATA 	  (MAX_PCIE_PAYLOAD - 36)	// max PCIe payload - WQE header
// Now, outstanding QP messages are protected by the spinlock
#define MAX_OUTSTANDING_SEND	1	//DISAGG_MAX_COMPUTE_NODE	// Fix to 1 for RDMA multiplexing
#define MAX_OUTSTANDING_READ	1	//DISAGG_MAX_COMPUTE_NODE	// Fix to 1 for RDMA multiplexing

#define IMM_RING_SIZE 1024*1024*4
#define IMM_PORT_CACHE_SIZE 1024*1024*4

#define SEND_REPLY_WAIT -101

#ifndef NOT_IMPORT_LINUX
enum mode {
	M_WRITE,
	M_READ,
};

#ifndef BF_CONTROLLER
struct cn_ib_mr {
	void			*addr;
	void 			*dma_addr;
	size_t			length;
	uint32_t		lkey;
	uint32_t		rkey;
	uint32_t		node_id;
};

struct _rdma_context_pad
{
	char x[0];
} ____cacheline_aligned_in_smp;

#define CTX_PADDING(name) struct _rdma_context_pad name;

struct rdma_context {
	struct ib_context	*context;
	struct ib_pd		*pd;
	struct ib_cq		**cq; // one completion queue for all qps
	struct ib_cq		**send_cq;
	struct ib_qp		**qp; // multiple queue pair for multiple connections
	spinlock_t 			conn_lock[NUM_PARALLEL_CONNECTION];
	atomic_t			next_inval_data_conn;
	atomic_t			next_inval_ack_conn;

	int			        send_flags;
	int			        rx_depth;
	struct ib_port_attr portinfo;
	int 			    num_connections;
	int                 num_node;
	int                 num_parallel_connection;
	atomic_t            *num_alive_connection;
	atomic_t		    num_alive_nodes;
	struct ib_mr        *proc;
	int                 node_id;
	int					ib_port_id;
    int                 psn;
    union ib_gid        gid;
    struct ib_gid_attr  gid_attr;

	void **local_rdma_recv_rings;
	struct cn_ib_mr *local_rdma_ring_mrs;

	//Needed for cross-nodes-implementation
	atomic_t num_completed_threads;
	
	CTX_PADDING(_pad1_)
	atomic_t *connection_count;

	//Mapped memory area for NIC
	void *mn_mapped_mem;
	struct cn_ib_mr mn_mapped_mem_mr;
};

struct dest_info {
	int node_id;
	int lid;
	int qpn[NUM_PARALLEL_CONNECTION];
	int psn;
	u64 base_addr;
	u64 size;
	u8 mac[ETH_ALEN];
	union ib_gid gid;
	u32 lkey;	// not needed but only for debug
	u32 rkey;
};

#define NUM_POLLING_THREADS		(1)

/* send poll thread model */
int fit_poll_recv_cq(void *_info);
// int waiting_queue_handler(void *_info);

inline void fit_free_recv_buf(void *input_buf);

int get_joined_nodes(void);

// allocate memory region
uintptr_t cn_reg_mr_addr(struct rdma_context *ctx, void *addr, size_t length);
void cn_unmap_mr_addr(struct rdma_context *ctx, u64 dma_addr, size_t length);
inline uintptr_t
cn_reg_mr_phys_addr(struct rdma_context *ctx, void *addr, size_t length);

/*
 * some additional message related functions
 */
unsigned int get_node_global_lid(unsigned int nid);
unsigned int get_node_first_qpn(unsigned int nid);

struct dest_info *set_node_info(unsigned int nid, int lid, u8 *mac, unsigned int *qpn, int psn, union ib_gid *gid);
struct dest_info *get_node_info(unsigned int nid);

#endif /* NOT_IMPORT_LINUX */

#endif /* BF_CONTROLLER */
#endif /* __NETWORK_FIT_DISAGGREGATION_H__ */
