/*
 * network_disagg.h
 *
 * This file is also included by the controller's source tree.
 * So adding more definitions or includes will fail
 * (unless you do ifndef __CONTROLLER__ or BF_CONTROLLER).
 */

#ifndef __NETWORK_DISAGGREGATION_H__
#define __NETWORK_DISAGGREGATION_H__

#ifndef NOT_IMPORT_LINUX
#ifndef BF_CONTROLLER
#include <linux/kernel.h>
#include <linux/init.h>
#include <rdma/ib_verbs.h>

#include <linux/net.h>
#include <net/sock.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <asm/uaccess.h>
#include <linux/socket.h>
#include <linux/slab.h>
#else
#define __aligned(X) __attribute__((aligned (X)))
#endif
#endif

#define _DISAGG_USE_RC_CONNECTION_

#define _RECV_CHECK_TIME_IN_JIFFIES 5
#define _MAX_CHECK_COUNTER  (HZ/_RECV_CHECK_TIME_IN_JIFFIES/2)
#define DISAGG_NET_SLEEP_RETRY_IN_MS 50
#define DISAGG_NET_SLEEP_RESET_IN_MS 500
#define DISAGG_NET_RDMA_TIMEOUT_IN_SEC 30
#define DISAGG_NET_TCP_TIMEOUT_IN_MS 10000
#define DISAGG_NET_ACK_TIMEOUT_IN_MS (2000)
#define DISAGG_NET_RDMA_MAX_RETRY 10
#define DISAGG_NET_TCP_MAX_RETRY 10
#define DISAGG_NET_TCP_MAX_RECV_RETRY 50
#define DISAGG_NET_CTRL_POLLING_SKIP_COUNTER 10000	// skip counter for control plane
#define DISAGG_NET_POLLING_SKIP_COUNTER 100000	// skip counter for data plane - can be increased
#define DISAGG_NET_RDMA_TIMEOUT_REPORT 1000000000	// 1 sec in ns
#define DISAGG_NET_POLLING_SHORT_BREAK 5000
#define DISAGG_NACK_RETRY_IN_USEC 50	// base back-off
#define DISAGG_NACK_MAX_BACKOFF 4
#define DISAGG_DELAY_FOR_RECOVERY 1000000 // in us
#define DISAGG_RECOVERY_COUNTER 10000
#define DISAGG_SLOW_ACK_REPORT_IN_USEC (DISAGG_NET_ACK_TIMEOUT_IN_MS * 1000)
#define DISAGG_SLOW_LOCK_REPORT_IN_USEC (4000)	// 1 ms

/* error codes */
#define ERR_DISAGG_NET_TIMEOUT          1
#define ERR_DISAGG_NET_CREATE_SOCKET    2
#define ERR_DISAGG_NET_CONN_SOCKET      3
#define ERR_DISAGG_NET_INCORRECT_BUF    4
#define ERR_DISAGG_NET_FAILED_TX        5
#define ERR_DISAGG_NET_INCORRECT_INT    6   // corrupted instruction or opcode

#define DISAGG_NET_MAXIMUM_BUFFER_LEN   (1024 * 1024)   // 1MB
// TODO(yash): Increase the length of this message.
//             Nowadays we return mappings, too!
#define DISAGG_NET_MAXIMUM_CTRL_LEN   	(4096)   // typical control msg size
// simple buffer size must be larger than sizeof(struct simple_reply)
#define DISAGG_NET_SIMPLE_BUFFER_LEN    (32)                    
#define DISAGG_NET_MAX_SIZE_ONCE        (4 * 1024)
#define DISAGG_NET_CRC_MARGIN           DISAGG_NET_SIMPLE_BUFFER_LEN   

/* ----------------- */
/* types of messages */
/* ----------------- */

/* RDMA message codes (passed to send_msg_to_memory_rdma) */


// y: Pulls data from the remote node at (vaddr).
#define DISAGG_PFAULT      		11
// Writes data to MN at ~(vaddr, data).
// The `req->data` field sent to send_rdma_write_data() uses kmapped (kernel)
// addresses, not DMA addresses.
#define DISAGG_DATA_PUSH   		21
// Target data that will forwarded to the requester.
// The `req->data` field sent to send_rdma_write_data() uses DMA addresses, not
// kernel mapped addresses. Never read from those buffers directly!
#define DISAGG_DATA_PUSH_TARGET		22 
#define DISAGG_DATA_PUSH_OTHER		24

// y: Unused.
#define DISAGG_DATA_PUSH_DUMMY		23

// y: These ACK codes are now unused. (Used to be needed for cache coherence,
//    not relevant for this branch's single-MN single-CN case).
#define DISAGG_ROCE_FIN_ACK		25
#define DISAGG_ROCE_INVAL_ACK		26
#define DISAGG_ROCE_EVICT_ACK		27

/* TCP message codes (passed to send_msg_to_control) */

#define DISAGG_FORK        		1
#define DISAGG_EXEC        		2
#define DISAGG_EXIT        		3
#define DISAGG_MMAP        		5
#define DISAGG_BRK         		6
#define DISAGG_MUNMAP      		7
#define DISAGG_REMOTE_MUNMAP      	17
#define DISAGG_MREMAP      		8
// y: These 2 messages are sent SCP->MN. Unimplemented now.
#define DISAGG_MEM_INIT			31
#define DISAGG_MEM_COPY			32
// This message sends the local node's RDMA metadata to the control plane.
// It announces the node's existence, type (compute vs memory), IP, base mem
// offset, etc. See: `struct rdma_msg_struct`.
#define DISAGG_RDMA         		51
#define DISAGG_REMOTE_THREAD		61
#define DISAGG_CONN_USAGE_REMOTE_FORK	62
#define DISAGG_LAUNCH_NEW_TASK		63
#define DISAGG_GEN_TGID			64
#define DISAGG_NEW_TGID			65
#define DISAGG_NEW_PID_NS		66
#define DISAGG_GET_TGID_IN_NS		67
#define DISAGG_FUTEX			71
// Used to allocate kshmem; kernel shared memory. This is memory that's
// allocated by the kernel, but can be addressed both in kernel and user space.
// eg: the buffers created by the `futex` syscall.
// This will necessarily create a new mapping.
#define DISAGG_KSHMEM_ALLOC		81		// kernel side shared memory
#define DISAGG_KSHMEM_BASE_ADDR		82
#define DISAGG_KSHMEM_ALLOC_VA		83
#define DISAGG_FIN_CONN    		99
#define DISAGG_ACC_REPORT    		100	// periodical access report
#define DISAGG_COPY_VMA    		101     // debugging purpose
// y: Unused.
#define DISAGG_CHECK_VMA   		102     // debugging purpose
#define DISAGG_DEBUG_KERN_LOG		103
#define DISAGG_DEBUG_KERN_STACK_LOG	104


// Virtual address field in RDMA
//  <------------------------- 64 bit ------------------------->
// |    16 bit    |    8 bit    |             40 bit            |
// |     PID      | Virtual address offset for exceptional case |

#define MN_VA_PID_SHIFT 			48 // first 16 bits
#define MN_VA_PID_BITS 				(64 - MN_VA_PID_SHIFT)
#define MN_VA_PID_BIT_MASK			0xffff000000000000
#define MN_VA_PARTITION_BASE	 	0x0000010000000000
#define MN_VA_PARTITION_BIT_MASK	0x0000ff0000000000
#define MN_VA_PARTITION_SHIFT		40	// last 40 bits
#define MN_VA_OFFSET_BIT_MASK 		0x000000ffffffffff

// cache invalidation related
#define CACHELINE_ROCE_OFFSET_TO_OPCODE 0
#define CACHELINE_ROCE_OFFSET_TO_FLAGS  1
#define CACHELINE_ROCE_OFFSET_TO_PKEY 	2
#define CACHELINE_ROCE_OFFSET_TO_QP		4	// Note: actually start from 5th bit so masking required
#define CACHELINE_ROCE_OFFSET_TO_ACKREQ	8
#define CACHELINE_ROCE_OFFSET_TO_VADDR 	12
#define CACHELINE_ROCE_OFFSET_TO_RKEY 	20
#define CACHELINE_ROCE_OFFSET_TO_DMALEN	24
#define CACHELINE_ROCE_VOFFSET_TO_IP 	24	// Reuse DMA length field (32 bits)
#define CACHELINE_ROCE_HEADER_LENGTH	32	// Total length
#define CACHELINE_BYPASS_MULTICAST_MASK 0xffff	// 16 bits of b'1
#define CACHELINE_ROCE_ACK_PREAMBLE 	0xffff
#define CACHELINE_ROCE_PSN_MASK		 	0xffffff
#define CACHELINE_ROCE_ACK_PREAMBLE_8U 	0xff
#define CACHELINE_ROCE_ACK_PROC_ACK		0x88	// Processed ACK
#define CACHELINE_ROCE_ACK_PROC_REQ		0x99	// Processed invalidation request
#define CACHELINE_ROCE_QP_EMBED_OFFSET 8	// Embedded QP (24-bit) in Rkey for RoCE-based invalidation ACK

#define DISAGG_INV_ACK_CIRC_BUF_SIZE (1024 * 1024 * 4)
#define CACHELiNE_ROCE_INVAL_BUF_LENGTH	(DISAGG_INV_ACK_CIRC_BUF_SIZE / CACHELINE_ROCE_HEADER_LENGTH)

// rkey field in RDMA
// R: reserved
// I: invalidation flag
// S, M: invalidation - shared or modified request
// D: invalidation - data push (back to requester) is requested
// QP: origianl QP from the requester
// |<----------------------------- 32 bit ---------------------------->|
// |     4      |    4     |     8      |  4  |1|1|1|1|   4    |   4   | <- in bits
// | permission | dir size |     QP     |  R  |D|M|S|I| (N)ACK | State |

// for RoCE and cacheline
#define MN_RKEY_PERMISSION_SHIFT	28
#define MN_RKEY_PERMISSION_MASK		0xf0000000
#define MN_RKEY_VM_EVICTION			0x4
// for cacheline
#define CACHELINE_ROCE_RKEY_STATE_MASK 	0xf			// last 4 digits
#define CACHELINE_ROCE_RKEY_NACK_MASK 	0x10		// 5th bit from right
// #define CACHELINE_ROCE_RKEY_INV_ACK 	0x80		// 6th bit from right
#define CACHELINE_ROCE_RKEY_INVALIDATION_MASK 0xF00 // 12 - 9th bit from right
#define CACHELINE_ROCE_RKEY_QP_MASK 	0xff0000	// 24 - 17th bit from right
#define CACHELINE_ROCE_RKEY_QP_SHIFT 	16			// 24 - 17th bit from right
#define CACHELINE_ROCE_RKEY_SIZE_MASK 	0xf000000	// 28 - 25th bit from right
#define CACHELINE_ROCE_RKEY_SIZE_SHIFT 	24			// 28 - 25th bit from right
#define CACHELINE_INVALIDATION_SHARED 	0x300		//0x100 | 0x200
#define CACHELINE_INVALIDATION_MODIFIED 0x500		//0x100 | 0x400
#define CACHELINE_INVALIDATION_DATA_REQ 0x800		//0x100 | 0x400
#define CACHELINE_ROCE_QP_SHARER_MASK 	0xffff		// last 16 digits
#define CACHELINE_ROCE_QP_INV_REQUESTER 0x10000		// invalidation for request, please update ack counter
#define CACHELINE_ROCE_QP_INV_ACK		0x1000000	// invalidation ACK in reserved field
#define ROCE_WRITE_REQ_OPCODE 10
#define ROCE_READ_REQ_OPCODE 12
#define ROCE_DEFAULT_FLAG 0x40
#define ROCE_DEFAULT_PKEY 0xffff
#define ROCE_REQ_SIZE 32
#define VM_INV_ACK 	0x7			// 31 - 28th bit

// Cacheline related in VA
#define CACHELINE_MIN_SHIFT 12
#define CACHELINE_MIN_SIZE (1 << CACHELINE_MIN_SHIFT)
#define CACHELINE_SHIFT 14	//21 for 2MB, 12 for 4KB, 14 for 16KB
#define CACHELINE_SIZE (1 << CACHELINE_SHIFT)
#define CACHELINE_MAX_SHIFT 21
#define CACHELINE_MAX_SIZE (1 << CACHELINE_MAX_SHIFT)
#define CACHELINE_PREFIX (64 - CACHELINE_SHIFT)
#define CACHELINE_MASK ((1 << CACHELINE_SHIFT) - 1)
//
//#define MN_VA_MIN_ADDR CACHELINE_MAX_SIZE
#define MN_VA_MIN_ADDR (1 << 30)
#define MN_DAC_MMAP_MIN_ADDR (1 << 16)

// port for TCP and UDP
#ifndef DEFAULT_PORT
#define DEFAULT_PORT 38001
#endif

#ifndef DEFAULT_UDP_PORT
#define DEFAULT_UDP_PORT 38002
#endif

#ifndef DEFAULT_UDP_SEND_PORT
#define DEFAULT_UDP_SEND_PORT 38003
#endif

#ifndef DEFAULT_ROCE_PORT
#define DEFAULT_ROCE_PORT 8791
#endif

#ifndef DEFAULT_ROCE_INVAL_PORT
#define DEFAULT_ROCE_INVAL_PORT 38005
#endif

#define _destport (DEFAULT_PORT)

#include "cluster_disagg.h"
// #define DISAGG_MEMORY_NODE_ID   1
// #define DISAGG_CONTROLLER_NODE_ID 0
// 
// #define DISAGG_COMPUTE_NODE_IP_START 201
// #define DISAGG_MEMORY_NODE_IP_START 221

#define DISAGG_MAGIC_NUMBER_FOR_TEST 0x1f2e3d4c

#define USER_SPACE_TCP

/* header for messaging */
#define MEMORY_HEADER_ALIGNMENT 8

#ifndef TCP_DAEMON
struct mem_header {
	uint32_t opcode;    // type of payload / message
	uint32_t sender_id; // id of the sender node
	uint32_t size;      // size of the payload (include this header)
} __aligned(MEMORY_HEADER_ALIGNMENT);

static inline struct mem_header *get_header_ptr(void *ptr)
{
	return (struct mem_header *)(ptr);
}

static inline void *get_payload_ptr(void *ptr)
{
	return (void *)((u8 *)ptr + sizeof(struct mem_header));
}

extern int get_local_node_id(void);
extern int set_local_node_id(int);

/*
 * Header for RDMA based message
 */
struct common_header {
	unsigned int opcode;
	unsigned int src_nid;		/* source nid */
	/*
	 * FIXME: Useless
	 */
	unsigned int length;
} __aligned(MEMORY_HEADER_ALIGNMENT);

static inline struct common_header *to_common_header(void *msg)
{
	return (struct common_header *)(msg);
}

static inline void *to_payload(void *msg)
{
	return (void *)((u8 *)msg + sizeof(struct common_header));
}

struct simple_reply {
    unsigned int ret;
} __aligned(MEMORY_HEADER_ALIGNMENT);

#ifndef BF_CONTROLLER



/* Pointer to RDMA messaging - only works if callback is registered by RDMA module */
int send_msg_to_memory(u32 msg_type, void *payload, u32 len_payload,
							void *retbuf, u32 max_len_retbuf);
// y: These functions take a kmapped address, and register a quick MR with the
//    ib device so we can DMA to/from that address. They return the new DMA
//    address that refers to the provided memory region.
inline unsigned long map_page_for_dma(void *addr);
inline unsigned long map_region_for_dma(void *addr, unsigned long size);

// Zero-init a local memory region. This function takes cn_va addresses as input.
int zero_rmem_region(struct task_struct *tsk, u64 addr, size_t len);

/* TCP based functions */
// Default TCP based implementation
u32 get_controller_ip(void);
u32 create_address(u8 *ip);
int send_msg_to_control(u32 msg_type, void *payload, u32 len_payload,
                        void *retbuf, u32 max_len_retbuf);
int tcp_send(struct socket *sock, const char *buf, 
                    const size_t length, unsigned long flags);
int tcp_receive(struct socket *sock, char *buf, size_t bufsize, 
                        unsigned long flags);
int tcp_initialize_conn(struct socket **conn_socket, 
                        u32 destip_32, u16 destport);
int tcp_finish_conn(struct socket *conn_socket);
int tcp_release_conn(struct socket *conn_socket);
int tcp_try_next_data_no_lock(void *retbuf, u32 max_len_retbuf);
int tcp_reset_conn(void);

/* UDP based functions */
int udp_receive(struct socket *sock, char *buf, size_t bufsize,
				unsigned long flags, struct sockaddr_in *tmp_addr);
int udp_send(struct socket *sock, char *buf, size_t bufsize,
			 unsigned long flags, struct sockaddr_in *tmp_addr);
int udp_initialize(struct socket **conn_socket, u16 destport, int bind);

/* TCP daemon functions */
int register_send_recv_buffer(char *from, int from_length, char *to, int to_length);
int read_from_buffer(char *to, int length);

/* RDMA RoCE API */

struct mind_rdma_reqs {
	struct mind_rdma_req *reqs;
	int num_reqs;
	u64 mem_server_base_raddr; // ALWAYS ADD THIS TO REMOTE ADDR BEFORE SENDING.
};

// Represents an ongoing MIND request. Only RoCE module should access fields.
//
// XXX: I can probably move the sge to a contiguous array to increase NIC cache hit rate.
//      But let's keep it simple for now.
struct mind_rdma_req {
	struct ib_rdma_wr rdma_wr; // Represents outstanding request.
	struct ib_sge sge;         // Represents the targeted local memory.
	int owning_cpu;            // y: who sent this?
	unsigned long send_time;
};

// API Callback Types
typedef u64 (*mind_rdma_map_dma_callback)(struct page *page, size_t len);
typedef void (*mind_rdma_unmap_dma_callback)(u64 laddr_dma, size_t len);
typedef void *(*mind_rdma_get_fhqp_handle_callback)(void);
typedef void *(*mind_rdma_get_cnqp_handle_callback)(void);
typedef int (*mind_rdma_put_qp_handle_callback)(void *qp_handle);
typedef int (*mind_rdma_read_callback)(void *qp_handle, void *buf, unsigned long addr, unsigned long len,
		struct mind_rdma_req *out);
typedef int (*mind_rdma_write_callback)(void *qp_handle, void *lbuf, unsigned long addr, unsigned long len,
		struct mind_rdma_req *out);
typedef int (*mind_rdma_read_sync_callback)(void *qp_handle, void *lbuf, u64 size, unsigned long remote_addr);
typedef int (*mind_rdma_write_sync_callback)(void *qp_handle, void *lbuf, u64 size, unsigned long remote_addr);
typedef void (*mind_rdma_initialize_batched_write_callback)(struct mind_rdma_reqs *out);
typedef int (*mind_rdma_batched_write_callback)(void *qp_handle, struct mind_rdma_reqs *reqs, int req_offset);
typedef int (*mind_rdma_check_cq_callback)(void *qp_handle, struct mind_rdma_req *target);
typedef int (*mind_rdma_check_cqs_callback)(void *qp_handle, int num_targets, struct ib_wc *wc_arr);
typedef int (*mind_rdma_poll_cq_callback)(void *qp_handle, struct mind_rdma_req *target);
typedef int (*mind_rdma_poll_cqs_callback)(void *qp_handle, struct mind_rdma_req *targets, int num_targets);

// Callback Setters
void set_mind_rdma_map_dma_fn(mind_rdma_map_dma_callback callbk);
void set_mind_rdma_unmap_dma_fn(mind_rdma_unmap_dma_callback callbk);
void set_mind_rdma_get_fhqp_handle_fn(mind_rdma_get_fhqp_handle_callback callbk);
void set_mind_rdma_get_cnqp_handle_fn(mind_rdma_get_cnqp_handle_callback callbk);
void set_mind_rdma_put_qp_handle_fn(mind_rdma_put_qp_handle_callback callbk);
void set_mind_rdma_read_fn(mind_rdma_read_callback callbk);
void set_mind_rdma_write_fn(mind_rdma_write_callback callbk);
void set_mind_rdma_read_sync_fn(mind_rdma_read_sync_callback callbk);
void set_mind_rdma_write_sync_fn(mind_rdma_write_sync_callback callbk);
void set_mind_rdma_initialize_batched_write_fn(mind_rdma_initialize_batched_write_callback callbk);
void set_mind_rdma_batched_write_fn(mind_rdma_batched_write_callback callbk);
void set_mind_rdma_check_cq_fn(mind_rdma_check_cq_callback callbk);
void set_mind_rdma_check_cqs_fn(mind_rdma_check_cqs_callback callbk);
void set_mind_rdma_poll_cq_fn(mind_rdma_poll_cq_callback callbk);
void set_mind_rdma_poll_cqs_fn(mind_rdma_poll_cqs_callback callbk);

// Declare Function Symbols
extern mind_rdma_map_dma_callback mind_rdma_map_dma_fn;
extern mind_rdma_unmap_dma_callback mind_rdma_unmap_dma_fn;
extern mind_rdma_get_fhqp_handle_callback mind_rdma_get_fhqp_handle_fn;
extern mind_rdma_get_cnqp_handle_callback mind_rdma_get_cnqp_handle_fn;
extern mind_rdma_put_qp_handle_callback mind_rdma_put_qp_handle_fn;
extern mind_rdma_read_callback mind_rdma_read_fn;
extern mind_rdma_write_callback mind_rdma_write_fn;
extern mind_rdma_read_sync_callback mind_rdma_read_sync_fn;
extern mind_rdma_write_sync_callback mind_rdma_write_sync_fn;
extern mind_rdma_initialize_batched_write_callback mind_rdma_initialize_batched_write_fn;
extern mind_rdma_batched_write_callback mind_rdma_batched_write_fn;
extern mind_rdma_check_cq_callback mind_rdma_check_cq_fn;
extern mind_rdma_check_cqs_callback mind_rdma_check_cqs_fn;
extern mind_rdma_poll_cq_callback mind_rdma_poll_cq_fn;
extern mind_rdma_poll_cqs_callback mind_rdma_poll_cqs_fn;

#endif // BF_CONTROLLER
#endif // TCP_DAEMON
#endif // __NETWORK_DISAGGREGATION_H__
