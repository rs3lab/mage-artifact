#define __SKIP_OFED_HEADERS__
#include <disagg/cnmap_disagg.h>
#include <disagg/network_disagg.h>
#include <disagg/network_rdma_disagg.h>
#include <disagg/exec_disagg.h>
#include <disagg/fault_disagg.h>
#include <disagg/cnthread_disagg.h>
#include <disagg/profile_points_disagg.h>
#include <linux/socket.h>
#include <asm/traps.h>

static unsigned char _destip[5] = {10,10,10,1,'\0'};

static const size_t _recv_buf_size = 2 * DISAGG_NET_MAX_SIZE_ONCE;
static spinlock_t send_msg_lock, recv_msg_lock;
static int _is_connected = 0;
static int _rdma_connected = 0;
static struct socket *_conn_socket = NULL;
static int disagg_computing_node_id = 1;
static void *flush_buf = NULL;
static atomic64_t ongoing_payload;

// From network_disagg.h
int get_local_node_id(void)
{
    return disagg_computing_node_id;
}
EXPORT_SYMBOL(get_local_node_id);

int set_local_node_id(int new_id)
{
    disagg_computing_node_id = new_id;
    return 0;
}
EXPORT_SYMBOL(set_local_node_id);

void __init disagg_network_init(void)
{
    spin_lock_init(&send_msg_lock);
    spin_lock_init(&recv_msg_lock);
    _is_connected = 0;
    _rdma_connected = 0;
    _conn_socket = NULL;
    flush_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
    atomic64_set(&ongoing_payload, 0);
}

u32 create_address(u8 *ip)
{
    u32 addr = 0;
    int i;

    for(i=0; i<4; i++)
    {
        addr += ip[i];
        if(i==3)
                break;
        addr <<= 8;
    }
    return addr;
}

u32 get_controller_ip(void)
{
    return htonl(create_address(_destip));
}
EXPORT_SYMBOL(get_controller_ip);

static int wait_socket_recv(struct socket *conn_socket)
{
    DECLARE_WAITQUEUE(recv_wait, current);
    int cnt = 0;
    
    if (!conn_socket)
        return -1;

    // spin_lock(&recv_msg_lock);
    add_wait_queue(&conn_socket->sk->sk_wq->wait, &recv_wait);  
    while(skb_queue_empty(&conn_socket->sk->sk_receive_queue) 
            && cnt <= _MAX_CHECK_COUNTER)
    {
        __set_current_state(TASK_INTERRUPTIBLE);
        cnt++;
        schedule_timeout(_RECV_CHECK_TIME_IN_JIFFIES);
    }
    __set_current_state(TASK_RUNNING);
    remove_wait_queue(&conn_socket->sk->sk_wq->wait, &recv_wait);
    // spin_unlock(&recv_msg_lock);
    return 0;
}

// This function was originally intended to serve as a general interface for
// the network. Packets would be provided to it, and automatically steered
// depending on their information.
//
// But nowadays, the packets routed to this function all just go to the switch
// control plane via TCP.
//
// Requests intended for the memory node (such as DISAGG_FAULT) won't go
// through this function.
//
// Here, the message type is useful for communicating intent with the control
// plane. But that may not be true for `send_msg_to_memory`.
int send_msg_to_control(u32 msg_type, void *payload, u32 len_payload,
                        void *retbuf, u32 max_len_retbuf)
{
    int ret = 0;
    u32 tot_len;
    void *msg = NULL, *payload_msg;
    // void *recv_buf = NULL;
    struct mem_header* hdr;
    int i = 0;
    unsigned long start_ts, end_ts;

    if (!retbuf)
        return -ERR_DISAGG_NET_INCORRECT_BUF;

    for (i = 0; i < DISAGG_NET_POLLING_SKIP_COUNTER; i++)
    {
        spin_lock(&send_msg_lock);
        if ((unsigned long)atomic64_read(&ongoing_payload))
        {
            spin_unlock(&send_msg_lock);
            // y: If this is converted to a `usleep_range`, the system
            //    complains about sleeping while under `TASK_UNINTERRUPTIBLE`.
            //    Ask yash to see `run-v7-atomic-sleeps` if you want more
            //    details.
            udelay(10); 
            // goto retry_lock;
        }else{
            goto have_lock;
        }
    }
    // problematic situation here
    // TODO: reset connection if needed
    printk(KERN_ERR "Msg timeout Initiate new connection\n");
    return -ERR_DISAGG_NET_TIMEOUT;

have_lock:

    atomic64_set(&ongoing_payload, (unsigned long)payload);   // I am the owner now
    spin_unlock(&send_msg_lock);
    // flush previous conn
    // if (likely(flush_buf != NULL))
    //     tcp_try_next_data_no_lock(flush_buf, PAGE_SIZE);

    // make header and attach payload
    tot_len = len_payload + sizeof(*hdr);
    // recv_buf = kmalloc(_recv_buf_size, GFP_KERNEL);
    msg = kmalloc(tot_len, GFP_KERNEL);
	// if (unlikely(!msg) || unlikely(!recv_buf)) {
    // if ((!msg) || (!recv_buf)) {
    if (!msg) {
		ret = -ENOMEM;
        goto out_sendmsg_err;
    }

    hdr = get_header_ptr(msg);
	hdr->opcode = msg_type;
    hdr->sender_id = get_local_node_id();
    hdr->size = tot_len;

    payload_msg = get_payload_ptr(msg);
	memcpy(payload_msg, payload, len_payload);
    // barrier();

    // send request
    ret = register_send_recv_buffer((char *)msg, tot_len, (char *) retbuf, max_len_retbuf);
    if (ret < tot_len){
        ret = -ERR_DISAGG_NET_FAILED_TX;
        goto out_sendmsg_err;
    }

    // simply polling response
    // printk(KERN_DEFAULT "Try to receive msg...\n");
    memset(retbuf, 0, max_len_retbuf);
    // PP_ENTER(NET_tcp_recv_msg);
    start_ts = jiffies;
    while (1)
    {
        for (i = 0; i < DISAGG_NET_CTRL_POLLING_SKIP_COUNTER; i++)
        {
            // wait_socket_recv(_conn_socket);
            // if(!skb_queue_empty(&_conn_socket->sk->sk_receive_queue))
            {
                ret = read_from_buffer((char *) retbuf, max_len_retbuf);
                if (ret > 0)
                    goto out_sendmsg;
            }
        }
        end_ts = jiffies;
        if ((end_ts > start_ts) && jiffies_to_msecs(end_ts - start_ts) > DISAGG_NET_TCP_TIMEOUT_IN_MS)
            break;
        // usleep_range(10, 10);
    }
    ret = -ERR_DISAGG_NET_TIMEOUT;
    printk(KERN_ERR "Msg timeout\n");

out_sendmsg:
    // if (msg_type == DISAGG_CHECK_VMA)
    //     PP_EXIT(NET_tcp_recv_msg);
out_sendmsg_err:
    // release connection  
    // if (_conn_socket && release_lock){
    //     _is_connected = 0;
    //     tcp_finish_conn(_conn_socket);
    //     _conn_socket = NULL;
    // }
    if ((unsigned long)atomic64_read(&ongoing_payload) != (unsigned long)payload)
    {
        // something went wrong
        printk(KERN_ERR "ongoing_payload does not match\n");
        ret = -ERR_DISAGG_NET_TIMEOUT;
    }else{
        atomic64_set(&ongoing_payload, 0);    // atomically finish the use
    }
    // spin_unlock(&send_msg_lock);
    if (msg)
        kfree(msg);
    // ret >= 0: size of received data, ret < 0: errors
    // barrier();
    return ret;
}
EXPORT_SYMBOL(send_msg_to_control);

int tcp_send(struct socket *sock, const char *buf, 
                    const size_t length, unsigned long flags)
{
        struct msghdr msg;
        //struct iovec iov;
        struct kvec vec;
        int len, written = 0, left = length;
        // mm_segment_t oldmm;

        //printk(KERN_DEFAULT "Send data: len %lu\n", length);

        msg.msg_name    = 0;
        msg.msg_namelen = 0;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags   = flags;

        // oldmm = get_fs(); set_fs(KERNEL_DS);
repeat_send:
        vec.iov_len = left;
        vec.iov_base = (char *)buf + written;

        len = kernel_sendmsg(sock, &msg, &vec, 1, left);
        if((len == -ERESTARTSYS) || 
            (!(flags & MSG_DONTWAIT) && (len == -EAGAIN)))
                goto repeat_send;

        // if(len > 0)
        // {
        //         written += len;
        //         left -= len;
        //         if(left)
        //                 goto repeat_send;
        // }
        // set_fs(oldmm);
        // return written ? written : len;
        return len;
}

// EXPORT_SYMBOL(tcp_send);

__always_inline int tcp_receive(struct socket *sock, char *buf, size_t bufsize, 
                        unsigned long flags)
{
        // mm_segment_t oldmm;
        struct msghdr msg;
        //struct iovec iov;
        struct kvec vec;
        int len;
        // int max_size = 50;

        msg.msg_name    = 0;
        msg.msg_namelen = 0;
        /*
        msg.msg_iov     = &iov;
        msg.msg_iovlen  = 1;
        */
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags   = flags;
        /*
        msg.msg_iov->iov_base   = str;
        msg.msg_ioc->iov_len    = max_size; 
        */
        vec.iov_len = bufsize;
        vec.iov_base = buf;

        // oldmm = get_fs(); set_fs(KERNEL_DS);
// read_again:
        //len = sock_recvmsg(sock, &msg, max_size, 0); 
        len = kernel_recvmsg(sock, &msg, &vec, 1, bufsize, flags);

        // if(len <= 0)
        // {
        //         // pr_info(" *** mtp | error while reading: %d | "
        //         //         "tcp_client_receive *** \n", len);
        //         goto read_again;
        // }

        //pr_info(" *** mtp | the server says: %s | tcp_client_receive *** \n", buf);
        // set_fs(oldmm);
        return len;
}

// EXPORT_SYMBOL(tcp_receive);

int tcp_initialize_conn(struct socket **conn_socket, 
                                u32 destip_32, u16 destport)
{
    int ret = -1;
    struct sockaddr_in saddr;
    int retry = 0;  //, yes = 1;

    ret = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, conn_socket);
    // ret = sock_create(PF_INET, SOCK_SEQPACKET, IPPROTO_TCP, conn_socket);
    if(ret < 0 || !(*conn_socket))
    {
        /* NULL POINTER ERROR */
        return -ERR_DISAGG_NET_CREATE_SOCKET;
    }
    pr_info("DISAGG_CONTAINER: tcp_initialize_conn: Socket created for addr 0x%0x\n",
            destip_32);

    // ret = ip_setsockopt((*conn_socket)->sk, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int));
    // if (ret < 0)
    // {
    //     /* NULL POINTER ERROR */
    //     return -ERR_DISAGG_NET_CREATE_SOCKET;
    // }

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(destport);
    saddr.sin_addr.s_addr = htonl(destip_32);

    while (1)
    {
        ret = (*conn_socket)->ops->connect(*conn_socket, (struct sockaddr *)&saddr,
                                        sizeof(saddr), O_RDWR);
        pr_info("tcp_initialize_conn: Try to connect...%d (%d/%d)\n", ret, retry, DISAGG_NET_TCP_MAX_RETRY);

        if(!ret || (ret == -EINPROGRESS)) {
            pr_info("tcp_initialize_conn: connection success!\n");
            break;  // success
        }
        retry ++;

        if (retry >= DISAGG_NET_TCP_MAX_RETRY)
        {
            pr_info(" *** mtp | Error: %d while connecting using conn "
                    "socket. | setup_connection *** \n", ret);
            return -ERR_DISAGG_NET_CONN_SOCKET;
        }
        // msleep(DISAGG_NET_SLEEP_RESET_IN_MS);
        
    }
    return 0;
}

int tcp_finish_conn(struct socket *conn_socket)
{
    struct mem_header* hdr;
    void *msg = kmalloc(sizeof(*hdr), GFP_KERNEL);
    
	if (unlikely(!msg)) {
		tcp_release_conn(conn_socket);
        return -ENOMEM;
	}

    hdr = get_header_ptr(msg);
	hdr->opcode = DISAGG_FIN_CONN;
    hdr->sender_id = get_local_node_id();
    hdr->size = sizeof(*hdr);

    // ignore error here, because we will relase the connection anyway
    tcp_send(conn_socket, msg, sizeof(*hdr), MSG_DONTWAIT);
    kfree(msg);
    return tcp_release_conn(conn_socket);
}

int tcp_release_conn(struct socket *conn_socket)
{
    if(conn_socket != NULL)
    {
        sock_release(conn_socket);
    }
    return 0;
}

int tcp_try_next_data_no_lock(void *retbuf, u32 max_len_retbuf)
{
    int ret = -1;
    // flush msg
    // spin_lock(&send_msg_lock);
    wait_socket_recv(_conn_socket);

    if(!skb_queue_empty(&_conn_socket->sk->sk_receive_queue))
    {
        memset(retbuf, 0, max_len_retbuf);
        ret = tcp_receive(_conn_socket, retbuf, max_len_retbuf, MSG_DONTWAIT);
        printk(KERN_DEFAULT "Get next msg (%d)\n", ret);
    }

    // spin_unlock(&send_msg_lock);
    return ret;
}

int tcp_reset_conn(void)
{
    return 0;
}

// ========== UDP listener ========== //
__always_inline int udp_receive(struct socket *sock, char *buf, size_t bufsize,
                                unsigned long flags, struct sockaddr_in *tmp_addr)
{
    // mm_segment_t oldmm;
    struct msghdr msg;
    //struct iovec iov;
    struct kvec vec;
    int len;
    // int max_size = 50;

    // get_cpu();
    msg.msg_name = tmp_addr;
    msg.msg_namelen = sizeof(*tmp_addr);
    /*
        msg.msg_iov     = &iov;
        msg.msg_iovlen  = 1;
        */
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = flags;
    /*
        msg.msg_iov->iov_base   = str;
        msg.msg_ioc->iov_len    = max_size; 
        */
    vec.iov_len = bufsize;
    vec.iov_base = buf;
    
    // read_again:
    //len = sock_recvmsg(sock, &msg, max_size, 0);
    len = kernel_recvmsg(sock, &msg, &vec, 1, bufsize, flags);
    // if (len > 0)
    // {
    //     if (msg.msg_name)
    //         pr_info("UDP packet received: %pI4\n", &(((struct sockaddr_in *)msg.msg_name)->sin_addr));
    //     else
    //         pr_info("UDP packet received without name\n");
    // }
    //pr_info(" *** mtp | the server says: %s | tcp_client_receive *** \n", buf);
    // set_fs(oldmm);
    // put_cpu();
    return len;
}

__always_inline int udp_send(struct socket *sock, char *buf, size_t bufsize,
                             unsigned long flags, struct sockaddr_in *tmp_addr)
{
    // mm_segment_t oldmm;
    struct msghdr msg;
    //struct iovec iov;
    struct kvec vec;
    int len;

    // let's put this outside of this function
    // get_cpu();
    msg.msg_name = tmp_addr;
    msg.msg_namelen = sizeof(*tmp_addr);
    /*
        msg.msg_iov     = &iov;
        msg.msg_iovlen  = 1;
        */
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = flags;
    /*
        msg.msg_iov->iov_base   = str;
        msg.msg_ioc->iov_len    = max_size; 
        */
    vec.iov_len = bufsize;
    vec.iov_base = buf;

retry:
    len = kernel_sendmsg(sock, &msg, &vec, 1, bufsize);
    if (((len > 0) && (len < bufsize)) || len == -EAGAIN)
        goto retry;
    // put_cpu();
    return len;
}

// EXPORT_SYMBOL(tcp_receive);

int udp_initialize(struct socket **conn_socket, u16 destport, int bind)
{
    int ret = -1;
    struct sockaddr_in saddr;
    int retry = 0; //, yes = 1;

    ret = sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP, conn_socket);
    if (ret < 0 || !(*conn_socket))
    {
        /* NULL POINTER ERROR */
        return -ERR_DISAGG_NET_CREATE_SOCKET;
    }

    // ret = ip_setsockopt((*conn_socket)->sk, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int));
    // if (ret < 0)
    // {
    //     /* NULL POINTER ERROR */
    //     return -ERR_DISAGG_NET_CREATE_SOCKET;
    // }
    if (bind) {
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(destport);
        saddr.sin_addr.s_addr = INADDR_ANY;

        while (1)
        {
            // ret = (*conn_socket)->ops->connect(*conn_socket, (struct sockaddr *)&saddr, sizeof(saddr), O_RDWR);
            ret = (*conn_socket)->ops->bind((*conn_socket), (struct sockaddr *)&saddr, sizeof(struct sockaddr));
            if (!ret || (ret == -EINPROGRESS))
            {
                break; // success
            }
            retry++;

            if (retry >= DISAGG_NET_TCP_MAX_RETRY)
            {
                // pr_info(" *** mtp | Error: %d while connecting using conn "
                //         "socket. | setup_connection *** \n", ret);
                return -ERR_DISAGG_NET_CONN_SOCKET;
            }
            msleep(DISAGG_NET_SLEEP_RESET_IN_MS);
        }
    }
    printk(KERN_DEFAULT "UDP listener initialized\n");
    return 0;
}

// ========== RDMA ========== //

// Temporary "DRAM Cache" for RDMA testing.
// Allocated statically here, because we can't make huge static allocations in a module.
// 2 GiB
#define MIND_RDMA_LMEM_SIZE_PAGES 5120
u8 mind_rdma_dram_cache[(4UL * 1024UL) * MIND_RDMA_LMEM_SIZE_PAGES] __aligned(PAGE_SIZE);
EXPORT_SYMBOL(mind_rdma_dram_cache);

// --------------------------------------------------
// SETTING RDMA CALLBACKS FOR USE W/ MODULES
// --------------------------------------------------

// DEFINE_AND_EXPORT_PP(NET_a);
// DEFINE_AND_EXPORT_PP(NET_b);
// DEFINE_AND_EXPORT_PP(NET_c);
// DEFINE_AND_EXPORT_PP(NET_d);

mind_rdma_map_dma_callback mind_rdma_map_dma_fn;
mind_rdma_unmap_dma_callback mind_rdma_unmap_dma_fn;
mind_rdma_get_fhqp_handle_callback mind_rdma_get_fhqp_handle_fn;
mind_rdma_get_cnqp_handle_callback mind_rdma_get_cnqp_handle_fn;
mind_rdma_put_qp_handle_callback mind_rdma_put_qp_handle_fn;
mind_rdma_read_callback mind_rdma_read_fn;
mind_rdma_write_callback mind_rdma_write_fn;
mind_rdma_read_sync_callback mind_rdma_read_sync_fn;
mind_rdma_write_sync_callback mind_rdma_write_sync_fn;
mind_rdma_initialize_batched_write_callback mind_rdma_initialize_batched_write_fn;
mind_rdma_batched_write_callback mind_rdma_batched_write_fn;
mind_rdma_check_cq_callback mind_rdma_check_cq_fn;
mind_rdma_check_cqs_callback mind_rdma_check_cqs_fn;
mind_rdma_poll_cq_callback mind_rdma_poll_cq_fn;
mind_rdma_poll_cqs_callback mind_rdma_poll_cqs_fn;

void set_mind_rdma_map_dma_fn(mind_rdma_map_dma_callback callbk)
{
    WRITE_ONCE(mind_rdma_map_dma_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_map_dma_fn);

void set_mind_rdma_unmap_dma_fn(mind_rdma_unmap_dma_callback callbk)
{
    WRITE_ONCE(mind_rdma_unmap_dma_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_unmap_dma_fn);

void set_mind_rdma_get_fhqp_handle_fn(mind_rdma_get_fhqp_handle_callback callbk)
{
    WRITE_ONCE(mind_rdma_get_fhqp_handle_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_get_fhqp_handle_fn);

void set_mind_rdma_get_cnqp_handle_fn(mind_rdma_get_cnqp_handle_callback callbk)
{
    WRITE_ONCE(mind_rdma_get_cnqp_handle_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_get_cnqp_handle_fn);

void set_mind_rdma_put_qp_handle_fn(mind_rdma_put_qp_handle_callback callbk)
{
    WRITE_ONCE(mind_rdma_put_qp_handle_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_put_qp_handle_fn);

void set_mind_rdma_read_sync_fn(mind_rdma_read_sync_callback callbk)
{
    WRITE_ONCE(mind_rdma_read_sync_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_read_sync_fn);

void set_mind_rdma_write_sync_fn(mind_rdma_write_sync_callback callbk)
{
    WRITE_ONCE(mind_rdma_write_sync_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_write_sync_fn);

void set_mind_rdma_initialize_batched_write_fn(mind_rdma_initialize_batched_write_callback callbk)
{
    WRITE_ONCE(mind_rdma_initialize_batched_write_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_initialize_batched_write_fn);

void set_mind_rdma_batched_write_fn(mind_rdma_batched_write_callback callbk)
{
    WRITE_ONCE(mind_rdma_batched_write_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_batched_write_fn);

void set_mind_rdma_read_fn(mind_rdma_read_callback callbk)
{
    WRITE_ONCE(mind_rdma_read_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_read_fn);

void set_mind_rdma_write_fn(mind_rdma_write_callback callbk)
{
    WRITE_ONCE(mind_rdma_write_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_write_fn);

void set_mind_rdma_check_cq_fn(mind_rdma_check_cq_callback callbk)
{
    WRITE_ONCE(mind_rdma_check_cq_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_check_cq_fn);

void set_mind_rdma_check_cqs_fn(mind_rdma_check_cqs_callback callbk)
{
    WRITE_ONCE(mind_rdma_check_cqs_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_check_cqs_fn);

void set_mind_rdma_poll_cq_fn(mind_rdma_poll_cq_callback callbk)
{
    WRITE_ONCE(mind_rdma_poll_cq_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_poll_cq_fn);

void set_mind_rdma_poll_cqs_fn(mind_rdma_poll_cqs_callback callbk)
{
    WRITE_ONCE(mind_rdma_poll_cqs_fn, callbk);
    smp_wmb();
}
EXPORT_SYMBOL(set_mind_rdma_poll_cqs_fn);


// --------------------------------------------------
// SENDING MESSAGES USING RDMA STACK
// --------------------------------------------------

// This is a legacy function, used only for some _control_ messages
// (eg: mmap, exec). It doesn't actually send pagefaults to mem, LOL.
// Don't use it in new functions.
//
// This function takes kmapped addresses, not DMA addresses.
static int send_pagefault_to_memory(
        u32 msg_type, void *payload, u32 len_payload,
        void *retbuf, u32 max_len_retbuf)
{
    struct fault_msg_struct *req = (struct fault_msg_struct *)payload;
    struct fault_reply_struct *reply = (struct fault_reply_struct *)retbuf;
    u64 laddr_dma;
    int ret;

    req->address = req->address & PAGE_MASK;
    pr_rdma("RDMA: Sending pagefault for 0x%lx to memory node w/ QP %d\n", req->address,
            current->qp_handle);

    BUG_ON(mind_rdma_read_sync_fn == NULL);
    BUG_ON(!mind_rdma_map_dma_fn || !mind_rdma_unmap_dma_fn);
    BUG_ON(!current->qp_handle);

    laddr_dma = mind_rdma_map_dma_fn(virt_to_page(reply->data), PAGE_SIZE);
    BUG_ON(!laddr_dma);

    BUG_ON(unlikely(mind_rdma_read_sync_fn == NULL));
    ret = mind_rdma_read_sync_fn(current->qp_handle, (void *)laddr_dma, PAGE_SIZE, req->address);

    mind_rdma_unmap_dma_fn(laddr_dma, PAGE_SIZE);

    reply->ret = 0;
    if (ret >= 0)
        reply->ret = DISAGG_FAULT_READ;
    return ret;
}

// This is a legacy function, used only for some _control_ messages
// (eg: mmap, exec). Don't use it in new functions.
//
// This function takes kmapped addresses, not DMA addresses.
static int send_data_push_to_memory(
        u32 msg_type, void *payload, u32 len_payload,
        void *retbuf, u32 max_len_retbuf)
{
    struct fault_data_struct *req = (struct fault_data_struct *)payload;
    struct fault_reply_struct *reply = (struct fault_reply_struct *)retbuf;
    u64 laddr_dma;
    int ret;

    pr_rdma("RDMA: Pushing 0x%lx, size=%u (buf=0x%lx) to memory node\n",
            req->address, req->data_size, (unsigned long) req->data);

    BUG_ON(req->address & ~PAGE_MASK);
    BUG_ON(((unsigned long) req->data) & ~PAGE_MASK);
    BUG_ON(req->data_size != PAGE_SIZE);
    BUG_ON(mind_rdma_write_sync_fn == NULL);
    BUG_ON(!mind_rdma_map_dma_fn || !mind_rdma_unmap_dma_fn);
    BUG_ON(!current->qp_handle);

    // Temporarily DMA-map the page so we can read/write to it.

    laddr_dma = mind_rdma_map_dma_fn(virt_to_page(req->data), PAGE_SIZE);
    BUG_ON(!laddr_dma);

    // XXX(yash): Hold on a minute. If this is page size anyways,
    // then what's the point of providing a size to this API?
    // Was it always like this? Is this why we were getting memory corruption
    // without the initial exec push to memory "hack"?
    ret = mind_rdma_write_sync_fn(current->qp_handle, (void *) laddr_dma, PAGE_SIZE, req->address);

    mind_rdma_unmap_dma_fn(laddr_dma, PAGE_SIZE);

    if (ret >= 0) // data_size SHOULD BE 4KB (=PAGE_SIZE)
        reply->ret = 0;

    return ret;
}

// This is a legacy function, used only for some _control_ messages
// (eg: mmap, exec). Don't use it in new functions.
//
// This function takes kmapped addresses, not DMA addresses.
int send_msg_to_memory(u32 msg_type, void *payload, u32 len_payload,
                                   void *retbuf, u32 max_len_retbuf)
{
    int ret = -ERR_DISAGG_NET_INCORRECT_INT;

    if (!retbuf)
        return -ERR_DISAGG_NET_INCORRECT_BUF;

    switch (msg_type) {
    case DISAGG_PFAULT:
        ret = send_pagefault_to_memory(msg_type, payload, len_payload, retbuf,
                max_len_retbuf);
        break;
    case DISAGG_DATA_PUSH:
    case DISAGG_DATA_PUSH_OTHER:
        ret = send_data_push_to_memory(msg_type, payload, len_payload, retbuf, max_len_retbuf);
        break;
    }
    return ret;
}
EXPORT_SYMBOL(send_msg_to_memory);


// --------------------------------------------------
// UTILITY FUNCTIONS
// --------------------------------------------------


inline unsigned long map_page_for_dma(void *addr)
{
    BUG_ON(!mind_rdma_map_dma_fn);
    return mind_rdma_map_dma_fn(addr, PAGE_SIZE);
}

inline unsigned long map_region_for_dma(void *addr, unsigned long size)
{
    BUG_ON(!mind_rdma_map_dma_fn);
    return mind_rdma_map_dma_fn(addr, size);
}

// This function fills a remote memory region with zeroes.
// Pass in the _local_ address of the memory region. 
int zero_rmem_region(struct task_struct *tsk, u64 addr, size_t len)
{
    unsigned long zero_addr, zero_addr_dma;
    u64 offset;

    BUG_ON(!PAGE_ALIGNED(addr));
    BUG_ON(len % PAGE_SIZE != 0);
    BUG_ON(mind_rdma_write_sync_fn == NULL);
    BUG_ON(!mind_rdma_map_dma_fn || !mind_rdma_unmap_dma_fn);
    BUG_ON(!tsk->qp_handle);

    zero_addr = get_zeroed_page(GFP_KERNEL);
    BUG_ON(!zero_addr);
    zero_addr_dma = mind_rdma_map_dma_fn(virt_to_page(zero_addr), PAGE_SIZE);
    BUG_ON(!zero_addr_dma);

    // Zero the page contents.
    for (offset = 0; offset < len; offset += PAGE_SIZE) {
        u64 raddr = get_cnmapped_addr(addr + offset);
        if (raddr == 0) {
            pr_err("zero_rmem_region: couldn't cnmap laddr 0x%llx!\n", addr + offset);
            BUG();
        }
        BUG_ON(mind_rdma_write_sync_fn(tsk->qp_handle, (void *) zero_addr_dma, PAGE_SIZE, raddr));
    }

    mind_rdma_unmap_dma_fn(zero_addr_dma, PAGE_SIZE);
    free_page(zero_addr);
    return 0;
}
