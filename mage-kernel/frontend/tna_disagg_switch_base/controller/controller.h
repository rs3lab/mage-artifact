#ifndef __NETWORK_SERVER_MODULE_H__
#define __NETWORK_SERVER_MODULE_H__

#include <stdbool.h>
#define __CONTROLLER__
#define BF_CONTROLLER
// #define BF_CONTROLLER_SWITCH_HW

#define DISAGG_MAX_NODE_CTRL 33 // 32 nodes + 1 controller
#define MAX_NUMBER_COMPUTE_NODE 16
#define DUMMY_MEM_NODE_ID (MAX_NUMBER_COMPUTE_NODE + 1)
#define MAX_NUMBER_PID_NS 256
#define MAX_NUMBER_MEMORY_NODE MAX_NUMBER_COMPUTE_NODE

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdint.h>

/* Spinlock */
#include <pthread.h>
#include "debug.h"

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define GFP_KERNEL 0
#define EPERM 1    /* Operation not permitted */
#define ENOENT 2   /* No such file or directory */
#define ESRCH 3    /* No such process */
#define EINTR 4    /* Interrupted system call */
#define EIO 5      /* I/O error */
#define ENXIO 6    /* No such device or address */
#define E2BIG 7    /* Argument list too long */
#define ENOEXEC 8  /* Exec format error */
#define EBADF 9    /* Bad file number */
#define ECHILD 10  /* No child processes */
#define EAGAIN 11  /* Try again */
#define ENOMEM 12  /* Out of memory */
#define EACCES 13  /* Permission denied */
#define EFAULT 14  /* Bad address */
#define ENOTBLK 15 /* Block device required */
#define EBUSY 16   /* Device or resource busy */
#define EEXIST 17  /* File exists */
#define EXDEV 18   /* Cross-device link */
#define ENODEV 19  /* No such device */
#define ENOTDIR 20 /* Not a directory */
#define EISDIR 21  /* Is a directory */
#define EINVAL 22  /* Invalid argument */
#define ENFILE 23  /* File table overflow */
#define EMFILE 24  /* Too many open files */
#define ENOTTY 25  /* Not a typewriter */
#define ETXTBSY 26 /* Text file busy */
#define EFBIG 27   /* File too large */
#define ENOSPC 28  /* No space left on device */
#define ESPIPE 29  /* Illegal seek */
#define EROFS 30   /* Read-only file system */
#define EMLINK 31  /* Too many links */
#define EPIPE 32   /* Broken pipe */
#define EDOM 33    /* Math argument out of domain of func */
#define ERANGE 34  /* Math result not representable */

// Errno
#define MAX_ERRNO 4095
#define IS_ERR_VALUE(x) ((x) >= (unsigned long)-MAX_ERRNO)

#ifndef LONG_MAX
#define LONG_MAX 0X7FFFFFFFFFFFFFFFL
#endif
#define ULONG_MAX (LONG_MAX * 2UL + 1UL)

#define DISAGG_VMA_MAX_SIZE 0x100000000 // 4GB

// Min/Max operation
#define min(x, y) (x <= y ? x : y)
#define max(x, y) (x >= y ? x : y)

// Variables for the compatibility with Linux kernel headers

struct task_struct; // We will use our own definition other than Linux's

struct pt_regs {
/*
 * C ABI says these regs are callee-preserved. They aren't saved on kernel entry
 * unless syscall needs a complete, fully filled "struct pt_regs".
 */
	unsigned long r15;
	unsigned long r14;
	unsigned long r13;
	unsigned long r12;
	unsigned long bp;
	unsigned long bx;
/* These regs are callee-clobbered. Always saved on kernel entry. */
	unsigned long r11;
	unsigned long r10;
	unsigned long r9;
	unsigned long r8;
	unsigned long ax;
	unsigned long cx;
	unsigned long dx;
	unsigned long si;
	unsigned long di;
/*
 * On syscall entry, this is syscall#. On CPU exception, this is error code.
 * On hw interrupt, it's IRQ number:
 */
	unsigned long orig_ax;
/* Return frame for iretq */
	unsigned long ip;
	unsigned long cs;
	unsigned long flags;
	unsigned long sp;
	unsigned long ss;
/* top of stack page */
};

// structure for connection
struct socket
{
    int sock_fd;
    // Set this field when we know that no further messages will ever come from
    // this TCP socket. (eg: this is a memory node that's done initializing).
    bool sock_is_done;
    struct sockaddr client_addr;
    socklen_t caddr_len;
    pthread_spinlock_t *sk_lock;
};

struct tcp_conn_info
{
    struct sockaddr_in *address;
    struct socket accept_socket;
    int thread_id;
    int is_send_conn;
    pthread_spinlock_t send_msg_lock;
};

struct server_service
{
    int running;
    struct socket listen_socket;
    struct sockaddr_in *address;
    pthread_t thread;
    const pthread_attr_t pth_attr;
};

extern int run_thread;

// Intialize, clear
int init_mn_man(void);
int clear_mn_man(void);
void run_controller_test(void);
void cacheline_init(void);
void cacheline_clear(void);
void task_spin_lock(void);
void task_spin_unlock(void);

// For multithreading
int init_remote_thread_man(void);
int clear_remote_thread_man(void);
void remote_thread_spin_lock(void);
void remote_thread_spin_unlock(void);

// TCP functions
int tcp_server_send(struct socket *, int id, const char *buf,
                    const size_t length, unsigned long flags);
int tcp_server_receive(struct socket *sk, void *buf,
                    int size, unsigned long flags);
int send_msg_to_mem(uint32_t msg_type, void *buf, unsigned long tot_len,
                    struct socket *sk,
                    void *reply_buf, unsigned long reply_size);

// Cache size management
void *cache_manager_main(void *args);

/* === Interfaces we defined in BF runtime === */
// Cacheline / directory
extern void bfrt_add_cacheline(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t c_idx);
extern void bfrt_del_cacheline(uint64_t vaddr, uint16_t vaddr_prefix);
extern void bfrt_set_cacheline_state(uint32_t cache_idx, uint16_t state);
extern void bfrt_reset_cacheline_state_on_update(uint32_t cache_idx);
extern void bfrt_add_cacheline_reg(uint32_t cache_idx, uint16_t state, uint16_t sharer,
                                   uint16_t dir_size, uint16_t dir_lock, uint32_t inv_cnt);
extern void bfrt_mod_cacheline_reg(uint32_t cache_idx, uint16_t state, uint16_t sharer,
                                   uint16_t dir_size, uint16_t dir_lock, uint32_t inv_cnt);
extern void bfrt_del_cacheline_reg(uint32_t cache_idx);

// Cache state transition
extern void bfrt_add_cachestate(uint8_t cur_state, uint8_t perm, uint8_t write_req,
                                uint8_t next_state, uint8_t reset_sharer, uint8_t send_inval);
extern void bfrt_add_cachesharer(const char *ip_addr, uint16_t sharer);
extern void bfrt_del_cachesharer(const char *ip_addr);
extern void bfrt_add_eg_cachesharer(const char *ip_addr, uint16_t sharer);

// Cache lock
extern void bfrt_set_cacheline_lock(uint32_t cache_idx, uint16_t dir_lock);
extern void bfrt_set_cacheline_inv(uint32_t cache_idx, uint32_t inv_cnt);

// NOTE: If you change this macro, change it in the CN too!
#define MAX_MAPS_IN_REPLY 128
// NOTE: If you change this struct, change it in the CN too!
struct mind_map_msg { 
    bool valid;
    uint64_t va;
    uint64_t mn_va;
    uint64_t size;
    uint16_t tgid;
} __packed;
void push_mind_map(uint64_t va, uint64_t size, uint16_t tgid, uint64_t va_to_dma);
int dump_mind_maps(struct task_struct *task, int pops_allowed, struct mind_map_msg *retbuf);

// Address translation
extern void bfrt_add_addr_trans( //uint32_t rkey,
    uint64_t vaddr, uint16_t vaddr_prefix,
    char *dst_ip_addr, uint64_t va_to_dma);
extern void bfrt_add_addr_except_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                                       char *dst_ip_addr, uint64_t va_to_dma,
                                       uint8_t permission, int account);
// Access control
extern void bfrt_modify_addr_except_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                                          char *dst_ip_addr, uint64_t va_to_dma, uint8_t permission);
extern void bfrt_del_addrExceptTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix, int account);

// RoCE request
extern void bfrt_add_roce_req(char *src_ip_addr, char *dst_ip_addr, uint32_t qp,
                              uint32_t new_qp, uint32_t rkey, uint16_t reg_idx);
// RoCE ack
extern void bfrt_add_roce_ack(uint32_t qp, // char *src_ip_addr,
                              uint32_t new_qp, char *new_ip_addr, uint16_t reg_idx);
// RoCE dummy ack for NACK
extern void bfrt_add_roce_dummy_ack(uint32_t qp, char *ip_addr,
                                    uint32_t new_qp, uint64_t vaddr);
// RoCE dummy ack forwarding for NACK
extern void bfrt_add_roce_ack_dest(uint32_t dummy_qp, char *ip_addr,
                                   uint32_t dest_qp_id, uint32_t dummy_qp_id);
// RoCE req to ack conversion
extern void bfrt_add_set_qp_idx(uint32_t qp_id, char *src_ip_addr,
                                uint16_t global_qp_idx);
extern void bfrt_add_sender_qp(uint32_t cpu_qp_id, char *src_ip_addr,
                               uint16_t mem_qp_id);
// Inval to Roce
extern void bfrt_add_egressInvRoute_rule(int nid, int inv_idx, uint32_t qp, uint32_t rkey,
                                         uint64_t vaddr, uint16_t reg_idx);
// Ack to Roce translation
extern void bfrt_add_ack_trans(char *dst_ip_addr, uint32_t qp, uint32_t new_qp,
                               uint32_t rkey, uint64_t vaddr, uint16_t reg_idx);
extern void print_bfrt_addr_trans_rule_counters(void);

#endif /* __NETWORK_SERVER_MODULE_H__ */
