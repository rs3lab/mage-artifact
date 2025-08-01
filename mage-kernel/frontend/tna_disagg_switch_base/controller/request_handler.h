#ifndef __MN_REQUEST_HANDLER_H__
#define __MN_REQUEST_HANDLER_H__

#include <stdio.h>
#include <stdbool.h>
#include "memory_management.h"
#include "controller.h"
#include "./include/disagg/network_disagg.h"
#include "./include/disagg/network_rdma_disagg.h"
#include "./include/disagg/network_fit_disagg.h"
#include "./include/disagg/fork_disagg.h"
#include "./include/disagg/launch_disagg.h"
#include "list_and_hash.h"
#include <pthread.h>

#define MN_PID_HASH_BIT         8
#define MN_CACHE_HASH_BIT       16  // will be extended to 16
#define RDMA_CTRL_GID "00000000000000000000ffff0a0a0a01" // hardcoded GID for 10.10.10.1

struct unique_tgid_node
{
    u32                 utgid;
    struct task_struct  *tsk;
    int                 local_ref;        // If it brecome 0, it should be freed
};

struct node_tgid_hash
{
    u16 node_id;
    u16 tgid;
    u16 pid_ns_id;
    // bool only_tgid_present;  // ZIMING: not sure if this is needed
    struct unique_tgid_node *utgid_node;
};

struct task_struct *mn_get_task(u16 tgid);
struct task_struct *mn_get_task_by_utgid(u32 utgid);

int mn_insert_new_task_mm(u16 sender_id, u16 tgid, u16 pid_ns_id, struct task_struct *tsk);
int mn_link_to_task_mm(u16 sender_id, u16 tgid, u16 pid_ns_id, u32 utgid);
int mn_delete_task(u16 sender_id, u16 tgid, u16 pid_ns_id);
void increase_utgid_ref(u16 tgid);
int get_utgid_ref(u16 tgid);

// Main functions for handling requests
struct socket;
int handle_fork(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_remote_fork(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_exec(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_exit(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_gen_tgid(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_get_new_tgid(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_create_new_pid_ns(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_get_tgid_in_ns(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_mmap(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_brk(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_munmap(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_mremap(struct mem_header *hdr, void *payload, struct socket *sk, int id);

int handle_pfault(struct mem_header *hdr, void *payload, struct socket *sk, int id);

// Debug
int handle_check(struct mem_header *hdr, void *payload);

// RDMA support: local version of dest_info (different than kernel's one)
struct dest_info
{
    int node_id;
    int lid;
    int qpn[NUM_PARALLEL_CONNECTION];
    int psn;
    u64 base_addr;
    u64 size;
    u64 ack_buf[NUM_PARALLEL_CONNECTION];
    u8 mac[ETH_ALEN];
    char gid[sizeof(DISAGG_RDMA_GID_FORMAT)];
    u32 lkey;
    u32 rkey;
    struct socket *sk;
    struct socket *out_sk;
};

struct mn_status
{
    int node_id;
    // TODO: struct list_head for the list of struct memory_node_mapping list
    struct dest_info *node_info;
    unsigned long alloc_size;
    // TODO: locking for parallel access
    struct list_node alloc_vma_list;
    spinlock_t alloc_lock;
};

void initialize_node_list(void);
struct dest_info *get_node_info(unsigned int nid);
int handle_client_rdma_init(struct mem_header *hdr, void *payload, struct socket *sk, int id, char *ip_addr, int *new_nid);
struct dest_info *ctrl_set_node_info(unsigned int nid, int lid, u8 *mac,
                                     unsigned int *qpn, int psn, char *gid,
                                     u32 lkey, u32 rkey,
                                     u64 addr, u64 size, u64 *ack_buf,
                                     struct socket *sk, bool is_mem_node);
int get_memory_node_num(void);
struct mn_status **get_memory_node_status(int *num_mn);
void increase_mem_node_count(void);

//RDMA version of main functions for handling requests
struct thpool_buffer;
int handle_fork_rdma(struct common_header *chdr, void *payload, struct thpool_buffer *tb);
int handle_exec_rdma(struct common_header *chdr, void *payload);
int handle_exit_rdma(struct common_header *chdr, void *payload, struct thpool_buffer *tb);
int handle_mmap_rdma(struct common_header *chdr, void *payload, struct thpool_buffer *tb);
int handle_brk_rdma(struct common_header *chdr, void *payload, struct thpool_buffer *tb);
int handle_munmap_rdma(struct common_header *chdr, void *payload, struct thpool_buffer *tb);
int handle_mremap_rdma(struct common_header *chdr, void *payload, struct thpool_buffer *tb);

int handle_pfault_rdma(struct common_header *chdr, void *payload, struct thpool_buffer *tb);
int handle_data_rdma(struct common_header *chdr, void *payload);
int handle_check_rdma(struct common_header *chdr, void *payload);

// Utils
char *get_memory_node_ip(int nid);
// int get_nid_from_ip_str(char *ip_addr, int is_mem_node);
int get_nid_from_ip_array(u8 *ip_addr, int is_mem_node);
void error_and_exit(const char *err_msg, const char *ftn, long line);

// Multithreading
void register_container_node_id(int sender_id);
int get_container_node_id(int idx);
int choose_new_thread_node_id(int sender_id, char *name, int ref_cnt);
int register_remote_thread_socket(u16 sender_id, struct socket *sk);
struct remote_thread_struct *get_remote_thread_socket(u16 sender_id);
int send_remote_thread_to_cn(int sender_id, int target_id,
                             struct task_struct *tsk,
                             u16 tgid, // u16 pid,
                             u16 pid_ns_id,
                             unsigned long clone_flags,
                             struct fork_msg_struct *fork_req,
                             struct socket *sk);
int send_new_task_to_cn(int target_id, struct socket *sk, char addr[]);

extern int num_container_nodes;
int send_msg_to_cn(u32 msg_type, struct socket *sk, void *payload, u32 len_payload,
                   void *retbuf, u32 max_len_retbuf);
#endif
