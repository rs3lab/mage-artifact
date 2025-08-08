#include "controller.h"
#include "memory_management.h"
#include "request_handler.h"
#include "cacheline_manager.h"
#include "cache_manager_thread.h"
#include "fault.h"
#include "list_and_hash.h"
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>


// Contains a list of remote threads...keyed by (node ID, TGID).


static struct hash_table remote_thread_hash; //[1 << MN_PID_HASH_BIT]
//static struct list_node remote_thread_socket_list;
static pthread_spinlock_t remote_thread_lock;

#define MAX_NUM_CONTAINER_NODE 16
static int container_node_ids[MAX_NUM_CONTAINER_NODE] = {0};
int num_container_nodes = 0;

void register_container_node_id(int sender_id) {
    if (num_container_nodes >= MAX_NUM_CONTAINER_NODE) {
        printf("fail to register container node\n");
        return;
    }
    container_node_ids[num_container_nodes] = sender_id;
    ++num_container_nodes;
    printf("container node[%d] = %d, new num_container_nodes: %d\n", num_container_nodes - 1, sender_id, num_container_nodes);
}

int get_container_node_id(int idx) {
    if (idx >= MAX_NUM_CONTAINER_NODE) {
        printf("fail to get registered container node id\n");
        return -1;
    }
    return container_node_ids[idx];
}

//TODO: heuristic/magic selection
// #define ROCKSDB_POLICY
#ifdef ROCKSDB_POLICY
#define ROCKSDB_NUM_INIT_THREADS 19
#endif

// #define GRAPHBOLT_POLICY

int choose_new_thread_node_id(int sender_id, char *name, int ref_cnt)
{
    if (strcmp(name, TEST_PROGRAM_NAME) == 0) {
#ifdef ROCKSDB_POLICY
        if (ref_cnt < ROCKSDB_NUM_INIT_THREADS || num_container_nodes <= 0)
            return sender_id;
        else
            return get_container_node_id((ref_cnt - ROCKSDB_NUM_INIT_THREADS) % num_container_nodes);
#elif defined GRAPHBOLT_POLICY
    if (num_container_nodes == 0)
        return sender_id;
    else {
        int next_id = ref_cnt % (num_container_nodes + 1);
        printf("next_id: %d, ref_cnt %d, num_container_nodes: %d\n", next_id, ref_cnt, num_container_nodes);
        if (next_id == 0)
            return sender_id;
        else
            return get_container_node_id((next_id - 1) % num_container_nodes);
    }
#else
    if (num_container_nodes == 0)
    {
        printf("next_id: %d, ref_cnt %d, num_container_nodes: %d\n", sender_id, ref_cnt, num_container_nodes);
        return sender_id;
    }
	// int next_id = ref_cnt % (num_container_nodes + 1);
    int next_id = ref_cnt % num_container_nodes;
    printf("next_id: %d, ref_cnt %d, num_container_nodes: %d\n", next_id, ref_cnt, num_container_nodes);
        if (ref_cnt < 0 || num_container_nodes <= 0) {
            printf("ref_cnt %d, num_container_nodes: %d, use local node instead\n", ref_cnt, num_container_nodes);
            return sender_id;
        } else {
            if (next_id == 0)
                return sender_id;
            else
                return get_container_node_id(next_id);
	        }
#endif
    } else {
        printf("Unknown program: %s\n", name);
        return sender_id;
    }
}

int register_remote_thread_socket(u16 sender_id, struct socket *sk)
{
    if (sender_id >= MAX_NUMBER_COMPUTE_NODE)
    {
        // out of range
        return -1;
    }

    struct remote_thread_struct *remote_thread_node;

    if (!sk)
    {
        fprintf(stderr, "Cannot insert NULL into the list\n");
        return -1;
    }

    remote_thread_node = ht_get(&remote_thread_hash, hash_ftn_u8(sender_id), sender_id);
    if(remote_thread_node) {
        fprintf(stderr, "Forward fork socket for remote id: %d already exists!\n", sender_id);
        return -1;

    }

    remote_thread_node = malloc(sizeof(struct remote_thread_struct));
    remote_thread_node->sender_id = sender_id;
    remote_thread_node->sk = sk;
    // FIXME check if need this list list_insert_at_head(&remote_thread_socket_list, remote_thread_node); // simply at the head
    ht_put(&remote_thread_hash, hash_ftn_u8(sender_id), sender_id, remote_thread_node);

    // depends on whether we use the first blade
#ifdef DSIABLE_FIRST_CONTAINER_BLADE
    if (sender_id != 1)
#endif
    register_container_node_id(sender_id);

    return 0;
}

int init_remote_thread_man(void)
{
    int i = 0;
    //TODO: initialize remote thread management

    // initialize compute node hash
    // only allocates variables inside not the hash table itself
    ht_create(&remote_thread_hash, 1 << MN_PID_HASH_BIT);
    pthread_spin_init(&remote_thread_lock, PTHREAD_PROCESS_PRIVATE);

    return 0;
}

int clear_remote_thread_man(void)
{
    int i = 0; //, j = 0;
    struct remote_thread_struct *remote_thread_ptr;

    for (i = 0; i < MAX_NUMBER_COMPUTE_NODE; i++) {
    	remote_thread_ptr = ht_get(&remote_thread_hash, hash_ftn_u8(i), i);
	if(remote_thread_ptr)
		free(remote_thread_ptr);
    }
    ht_free(&remote_thread_hash); //it frees hash node and its value (i.e., node_tgid_hash)
    printf("Hash lists cleared\n");

    return 0;
}

void remote_thread_spin_lock(void)
{
    pthread_spin_lock(&remote_thread_lock);
}

void remote_thread_spin_unlock(void)
{
    pthread_spin_unlock(&remote_thread_lock);
}

/* Mode selection */
extern int is_debug_mode(void);
extern int is_recording_mode(void);

int send_msg_to_cn(u32 msg_type, struct socket *sk, void *payload, u32 len_payload,
                   void *retbuf, u32 max_len_retbuf)
{
    int ret = -1;
    int tot_len;
    void *msg = NULL, *payload_msg;
    struct mem_header *hdr;
    // FIXME this id is not used in tcp_server_send yet
    int dummy_id = 0;
    int i = 0;
    pthread_spinlock_t *send_msg_lock;

    send_msg_lock = sk->sk_lock;
    if (!send_msg_lock) {
        printf("remote thread - can not get the socket lock\n");
    } else {
        printf("sk lock good\n");
    }

    // check for replaying mode
	if (is_debug_mode() && !is_recording_mode())
	{
		// we are replaying now, so do not send anything
		return len_payload;
	}

    if (!retbuf)
        return -ERR_DISAGG_NET_INCORRECT_BUF;

    if (!sk)
        return -1;

    // make header and attach payload
    tot_len = len_payload + sizeof(*hdr);
    msg = malloc(tot_len);
    if (!msg)
    {
        return -ENOMEM;
    }
    hdr = get_header_ptr(msg);
    hdr->opcode = msg_type;
    hdr->sender_id = DISAGG_CONTROLLER_NODE_ID; // Which send id is this?

    payload_msg = get_payload_ptr(msg);
    memcpy(payload_msg, payload, len_payload);

    // send request
    // ret = tcp_server_send(sk, whatever, msg, tot_len, MSG_DONTWAIT);
    pthread_spin_lock(send_msg_lock);
    ret = tcp_server_send(sk, dummy_id, (const char *)msg, tot_len, MSG_DONTWAIT);
    if (ret < tot_len)
    {
        ret = -ERR_DISAGG_NET_FAILED_TX;
        printf("munmap, expect to send[%d], but send[%d]\n", tot_len, ret);
        goto msg_out;
    }

    while (1)
    {
        for (i = 0; i < DISAGG_NET_POLLING_SKIP_COUNTER; i++)
        {
            ret = tcp_server_receive(sk, retbuf, max_len_retbuf, MSG_DONTWAIT);
            if (ret > 0) {
                goto msg_out;
            }
        }
        usleep(1000); // 1 ms
    }

msg_out:
    pthread_spin_unlock(send_msg_lock);
    if (msg)
    {
        free(msg);
    }
    return ret;
}

// FIXME not used
//static int send_simple_ack(struct socket *sk, int id, int ret)
//{
//	// TEMPORARY
//	const int len = DISAGG_NET_SIMPLE_BUFFER_LEN;
//	char out_buf[DISAGG_NET_SIMPLE_BUFFER_LEN + 1];
//	memset(out_buf, 0, len + 1);
//	sprintf(out_buf, "ACK %d", ret);
//	tcp_server_send(accept_socket, id, out_buf,
//					strlen(out_buf), MSG_DONTWAIT);
//	return 0;
//}

int send_new_task_to_cn(int target_id, struct socket *sk, char addr[])
{
    int ret;
    char retbuf[64];
    struct launch_task_struct launch_task_req;
    strncpy(launch_task_req.addr, addr, strlen(addr) + 1);
    
    printf("addr: %s\n", addr);
    printf("launch_task_req.addr: %s\n", launch_task_req.addr);
    size_t tot_size = sizeof(struct launch_task_struct);

    ret = send_msg_to_cn(DISAGG_LAUNCH_NEW_TASK, sk, &launch_task_req, tot_size, retbuf, 64);
    return ret;
}

int send_remote_thread_to_cn(int sender_id, int target_id,
                             struct task_struct *tsk,
                             u16 tgid, // u16 pid,
                             u16 pid_ns_id,
                             unsigned long clone_flags,
                             struct fork_msg_struct *fork_req,
                             struct socket *sk)
{
    int ret;
    char retbuf[64];
    pthread_spinlock_t *send_msg_lock;
    //struct remotethread_reply_struct *ret_buf_ptr;

    if (!sk) {
        printf("fail to get socket to forward remote thread\n");
    }
    if (!sk || !tsk)
    {
        printf("remote thread - can not get the socket to send requests\n");
    } else {
        printf("sk and tsk good\n");
    }

    send_msg_lock = sk->sk_lock;

    if (!fork_req) {
        printf("no fork req to forward\n");
    } else {
        printf("fork_req good\n");
    }
    memset(retbuf, 0, sizeof(retbuf));
    if (!send_msg_lock) {
        printf("remote thread - can not get the socket lock\n");
    } else {
        printf("sk lock good\n");
    }
    //send and recv remote thread msg
    printf("sizeof(struct fork_msg_struct): [%lu], sizeof(struct file_mapping_info): [%lu], (fork_req->num_file_mappings - 1): %u\n",  sizeof(struct fork_msg_struct), sizeof(struct file_mapping_info), fork_req->num_file_mappings - 1);
    size_t tot_size = sizeof(struct fork_msg_struct) + 
        (fork_req->num_file_mappings - 1) * sizeof(struct file_mapping_info);
    printf("remote thread forwarded size[%lu]\n", tot_size);

    ret = send_msg_to_cn(DISAGG_REMOTE_THREAD, sk, fork_req, tot_size, retbuf, 64);

    //>=0 means success
    if (ret >= 0)
    {
        ////first remote thread, its tgid is recorded
        //ret_buf_ptr = (struct remotethread_reply_struct *)retbuf;
        //if (!tsk->tgids[target_id])
        //{
        //    // printf("remote node %d's tgid is: %d\n",
        //    //        target_id, ((struct remotethread_reply_struct *)retbuf)->tgid);
        //    tsk->tgids[target_id] = ret_buf_ptr->tgid;
        //}

        //// TODO: add remotethread pairs
        //// add_remotethread_pair(tsk, sender_id, pid, target_id,
        ////                       ((struct remotethread_reply_struct *)retbuf)->pid);
        //printf("new remotethread pair: master(%d,%d)<--->remote(%d,%d)\n",
        //       sender_id, tgid, target_id, ret_buf_ptr->tgid);
        ret = 0;
    }
    else
    {
        printf("fail to send new thread msg to cn, ret: %d\n", ret);
        ret = -1;
    }



    return ret;
}

/* Functions for handling (node id, tgid) -> (unique tgid) mapping */
struct remote_thread_struct *get_remote_thread_socket(u16 sender_id)
{
    struct remote_thread_struct *hnode = NULL;

    if (sender_id >= MAX_NUMBER_COMPUTE_NODE)
    {
        // out of range
        return NULL;
    }

    hnode = ht_get(&remote_thread_hash, hash_ftn_u8(sender_id), sender_id);
    return hnode;
}

