#include "controller.h"
#include "memory_management.h"
#include "kshmem.h"
#include "request_handler.h"
#include "fault.h"
#include "pid_ns.h"
#include "list_and_hash.h"
#include "config.h"
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include "./include/disagg/exit_disagg.h"
#include "./include/disagg/config.h"
#include "./include/disagg/kshmem_disagg.h"
#include "./include/disagg/pid_disagg.h"
#include "./include/disagg/pid_ns_disagg.h"

/* Misc functions */

// Copies the existing mind CN VA -> MN VA addr mappings to the
// Returns the number of maps exported, or 0 if there was an error.
int export_mind_maps(uint16_t tgid, struct mm_struct *mm, struct mind_map_msg *retbuf, int retbuf_size)
{
    printf("Exporting mind maps...\n");
    int ret = 0;

    memset(retbuf, 0, sizeof(*retbuf) * retbuf_size);

    struct vm_area_struct *vma = mm->mmap;
    for (; vma; vma = vma->vm_next) {
        if (ret >= retbuf_size) {
             printf("WARN: couldn't export all mind maps\n");
             break;
        }

        struct memory_node_mapping *vma_mapping = vma->vm_private_data;
        if (!vma_mapping) // return only allocated VMAs.
             continue;

        struct mind_map_msg *out = &retbuf[ret];
        out->valid = true;
        out->va = vma->vm_start;
        // We _don't_ include the MN base MR offset. The CN will deal with that.
        out->mn_va = vma_mapping->base_addr;
        out->size = vma_mapping->size;
        out->tgid = tgid;

        printf("\texporting cnmap: va=0x%lx, mn_va=0x%lx, size=%lu, tgid=0x%u\n", out->va, out->mn_va, out->size, out->tgid);

        ret++;
    }
    printf("Done exporting %d mind maps...\n", ret);
    return ret;
}

#define pr_info(...) printf(__VA_ARGS__)
/*
 * =====  Message handlers =====
 */
static int __handle_fork(struct mem_header *hdr, void *payload, struct fork_reply_struct *reply)
{
    struct fork_msg_struct *fork_req = (struct fork_msg_struct *)payload;

    printf("__handle_fork received: sender: %u, tgid: %u, pid_ns_id: %u, pid: %u\n",
                       (unsigned int)hdr->sender_id, (unsigned int)fork_req->tgid, (unsigned int)fork_req->pid_ns_id,
                       (unsigned int)fork_req->pid);

    int ret = 0;
    int success = 0;
    struct task_struct *old_tsk, *cur_tsk;
    task_spin_lock();
    cur_tsk = mn_get_task(fork_req->tgid);
    if (!cur_tsk) // no existing entry
    {

        printf("FORK: new process: sender_id, %u, tgid: %u, ns: %u, get_utgid_ref(fork_req->tgid): %d\n",
                hdr->sender_id, (unsigned int)fork_req->tgid, (unsigned int)fork_req->pid_ns_id,
                get_utgid_ref(fork_req->tgid));

        put_tgid_to_ns_table(fork_req->tgid, fork_req->pid_ns_id); // Double insert
        // 1) Initial for from systemd
        old_tsk = mn_get_task(fork_req->prev_tgid);
        if (!old_tsk)
        { // no prev entry
            printf("__handle_fork: mn_create_dummy_task_mm: sender: %u, tgid: %u, pid_ns_id: %u, pid: %u\n",
                       (unsigned int)hdr->sender_id, (unsigned int)fork_req->tgid, (unsigned int)fork_req->pid_ns_id,
                       (unsigned int)fork_req->pid);

            ret = mn_create_dummy_task_mm(hdr->sender_id, fork_req->tgid, fork_req->pid_ns_id, fork_req->pid);
            if (!ret)
            {
                printf("Dummy task/mm inserted (exec required): sender: %u, tgid: %u, pid_ns_id: %u, pid: %u\n",
                       (unsigned int)hdr->sender_id, (unsigned int)fork_req->tgid, (unsigned int)fork_req->pid_ns_id,
                       (unsigned int)fork_req->pid);
                ret = -ERR_DISAGG_FORK_NO_PREV;
            }
        }
        else
        {
            // 2) Normal fork
            printf("__handle_fork: mn_create_mm_from: sender: %u, tgid: %u, pid_ns_id: %u, pid: %u\n",
                       (unsigned int)hdr->sender_id, (unsigned int)fork_req->tgid, (unsigned int)fork_req->pid_ns_id,
                       (unsigned int)fork_req->pid);
            ret = mn_create_mm_from(hdr->sender_id, fork_req->tgid, fork_req->pid_ns_id, fork_req->pid,
                                    old_tsk, fork_req->clone_flags);
            if (!ret)
            {
                printf("Copied task/mm inserted: sender: %u, tgid: %u, pid: %u, prev_tgid: %u, prev_pid: %u\n",
                       (unsigned int)hdr->sender_id,
                       (unsigned int)fork_req->tgid, (unsigned int)fork_req->pid,
                       (unsigned int)fork_req->prev_tgid, (unsigned int)fork_req->prev_pid);
            }
            else {
                printf("__handle_fork: mn_create_mm_from: failed\n");
            }
            cur_tsk = mn_get_task(fork_req->tgid);

            if (!cur_tsk) {
                printf("__handle_fork cur_tsk not found!!\n");
            }
        }
    }
    else
    {
        // We have existing processes - can be local or remote
        long target_node;
        printf("FORK: existing process: sender_id, %u, tgid: %u, ns: %u, get_utgid_ref(fork_req->tgid): %d\n", hdr->sender_id, (unsigned int)fork_req->tgid, (unsigned int)fork_req->pid_ns_id, get_utgid_ref(fork_req->tgid));
        ret = 0;
        target_node = choose_new_thread_node_id(hdr->sender_id, fork_req->comm,
                                                get_utgid_ref(fork_req->tgid)); // FIXME this comm is for process name?
        struct remote_thread_struct *cur_sk;
        remote_thread_spin_lock();

        if (target_node != hdr->sender_id) // will be in remote
        {
            printf("master node id is: %d, new thread runs on node id: %ld\n",
                   hdr->sender_id, target_node);
            cur_sk = get_remote_thread_socket(target_node);
            if (!cur_sk)
            {
                printf("fail to get socket to forward remote thread\n");
            }
            // TODO: reconsider this, currently only master node are allowed to launch new thread
            ret = send_remote_thread_to_cn(hdr->sender_id,
                                           target_node,
                                           cur_tsk,
                                           fork_req->tgid,
                                           fork_req->pid_ns_id,
                                           (unsigned long)fork_req->clone_flags,
                                           fork_req,
                                           cur_sk->sk);
            // 0 means success
            if (!ret)
            {
                ret = -ERR_DISAGG_FORK_REMOTE_THREAD;
                success = 1;
                // cur_tsk->tgids[target_node] = 1; 
                cur_tsk->tgids[target_node] = fork_req->tgid; 
            }
            else
                printf("fail to send remote thread to cn, err is: %d\n", ret);
        }
        else
        {
            printf("new thread runs on master node id: %d\n", hdr->sender_id);
            success = 1;
            ret = -ERR_DISAGG_FORK_THREAD;
        }
        if (success)
            cur_tsk->ref_cnt[target_node]++;
        remote_thread_spin_unlock();
    }

    if (cur_tsk) {
        printf("task(tgid: %u)->ref_cnt[hdr->sender_id]:%u\n", cur_tsk->tgid, cur_tsk->ref_cnt[hdr->sender_id]);

        export_mind_maps(fork_req->tgid, cur_tsk->mm, (struct mind_map_msg *) reply->maps, MAX_MAPS_IN_REPLY);
    }
    if (success)
        increase_utgid_ref(fork_req->tgid);

    task_spin_unlock();

    reply->ret = ret;
    reply->vma_count = 0; // TODO: not used for now
    printf("__handle_fork ret: %d\n", ret);
    return ret;
}

static int __handle_remote_fork(struct mem_header *hdr, void *payload, struct fork_reply_struct *reply, struct socket *sk)
{
    int ret = -1;
    remote_thread_spin_lock();
    pr_info("Receive handling remote fork message\n");
    ret = register_remote_thread_socket(hdr->sender_id, sk);
    pr_info("Successfully register remote fork\n");
    remote_thread_spin_unlock();
    reply->ret = ret;
    reply->vma_count = 0; // TODO: not used for now
    return ret;
}

int handle_fork(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct fork_reply_struct reply;
    memset(&reply, 0, sizeof(reply));
    int ret = __handle_fork(hdr, payload, &reply);
    printf("handle_fork ret: %d\n", ret);
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    printf("handle_fork: tcp_server_send, reply->ret: %d\n", reply.ret);
    return ret;
}

int handle_remote_fork(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct fork_reply_struct reply;
    int ret = __handle_remote_fork(hdr, payload, &reply, sk);
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}

static int __handle_exec(struct mem_header *hdr, void *payload,
                         struct exec_reply_struct *reply)
{
    struct exec_msg_struct *exec_req = (struct exec_msg_struct *)payload;
    printf("__handle_exec received: sender: %u, tgid: %u, pid_ns_id: %u, pid: %u\n",
                       (unsigned int)hdr->sender_id, (unsigned int)exec_req->tgid, (unsigned int)exec_req->pid_ns_id,
                       (unsigned int)exec_req->pid);

    int ret = -1;
    // Update PID namespace mappings
    put_tgid_to_ns_table(exec_req->tgid, exec_req->pid_ns_id);
    ret = mn_update_mm(hdr->sender_id, exec_req->tgid, exec_req->pid_ns_id, exec_req);
    printf("__handle_exec received: after mn_update_mm\n");

    // TODO: do we need check the error here and free the task and mm structures?
    // Maybe when they are killed in computing node, it also be notified
    reply->ret = ret;
    reply->vma_count = 0;
    if (!ret)
         reply->vma_count = exec_req->num_vma;

    struct task_struct *tsk = mn_get_task(exec_req->tgid);
    export_mind_maps(exec_req->tgid, tsk->mm, (struct mind_map_msg *) &reply->maps, MAX_MAPS_IN_REPLY);

    return ret;
}

int handle_exec(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct exec_reply_struct reply;
    memset(&reply, 0, sizeof(reply));
    int ret = __handle_exec(hdr, payload, &reply);
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}

static int __handle_exit(struct mem_header *hdr, void *payload, struct exit_reply_struct *reply)
{
    struct exit_msg_struct *exit_req = (struct exit_msg_struct *)payload;
    struct task_struct *tsk = mn_get_task(exit_req->tgid);
    int ret = 0;

    task_spin_lock();

    if (tsk) // Not needed but only for checking ret
    {
        int i = 0;
        printf("EXIT: Before: task(tgid: %u) sender_id: %u, tsk->tgids[hdr->sender_id]: %u, task->ref_cnt[hdr->sender_id]: %u\n", tsk->tgid, hdr->sender_id, tsk->tgids[hdr->sender_id], tsk->ref_cnt[hdr->sender_id]);
        
        if ((exit_req->flag == EXIT_FIRST_TRY) && (tsk->tgids[hdr->sender_id]))
        {
            tsk->ref_cnt[hdr->sender_id]--;
            
            if (tsk->ref_cnt[hdr->sender_id] <= 0) // only when there is no remaining threads
            {
                tsk->tgids[hdr->sender_id] = 0; // FIXME should it be called per thread not blade?
            }
            // ret = mn_exit(hdr->sender_id, exec_req->tgid, tsk);
            /*
            for (i = 0; i < MAX_NUMBER_COMPUTE_NODE + 1; i++)
            {
                printf("tsk->tgids[%d]: %d\n", i, tsk->tgids[i]);
            }
            */
        }
        printf("EXIT: After: task(tgid: %u) sender_id: %u, tsk->tgids[hdr->sender_id]: %u, task->ref_cnt[hdr->sender_id]: %u\n", tsk->tgid, hdr->sender_id, tsk->tgids[hdr->sender_id], tsk->ref_cnt[hdr->sender_id]);

        for (i = 0; i < MAX_NUMBER_COMPUTE_NODE + 1; i++)
        {
            if (((int)(hdr->sender_id) != i) && tsk->tgids[i])
            {
                ret = -ERR_DISAGG_REMOTE_WAIT_EXIT;

                printf("EXIT: Other nodes id: %u, task->ref_cnt[i]: %u, tsk->tgids[i]: %d\n", i, tsk->ref_cnt[i], tsk->tgids[i]);
            }
        }

        if (!ret /*&& tsk->ref_cnt[hdr->sender_id] <= 0*/) // now all REMOTE threads have been called exit at least once
        {
            printf("EXIT: hdr->sender_id: %d, exit_req->tgid: %d, exit_req->pid_ns_id: %d\n", hdr->sender_id, (int)exit_req->tgid, exit_req->pid_ns_id);
            mn_delete_task(hdr->sender_id, exit_req->tgid, exit_req->pid_ns_id);
        }
        reply->ret = ret;
	    printf("EXIT: nid: %d, tgid: %d, ret: %d\n", (int)hdr->sender_id, (int)exit_req->tgid, ret);
    }else{
        reply->ret = -ERR_DISAGG_EXIT_NO_TASK; // no task found
        printf("EXIT: cannot find tgid: %d\n", (int)exit_req->tgid);
    }
    // task_struct_spin_unlock(tsk);
    task_spin_unlock();

    return ret;
}

int handle_exit(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct exit_reply_struct reply;
    int ret = __handle_exit(hdr, payload, &reply);
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}

// Added from pid_ns.c
int __handle_get_new_tgid(struct mem_header *hdr, void *payload, struct tgid_reply_struct *reply) 
{
    struct tgid_msg_struct *tgid_req = (struct tgid_msg_struct *)payload;
    printf("__handle_get_new_tgid: sender_id: %d, pid_ns_id: %d\n", tgid_req->sender_id, tgid_req->pid_ns_id);
    u16 tgid = get_new_tgid(tgid_req->sender_id, tgid_req->pid_ns_id);
    reply->tgid = tgid;
    // reply->tgid_ns = get_tgid_in_ns(tgid, tgid_req->pid_ns_id);
    reply->ret = 0;
    return 0;
}

int handle_get_new_tgid(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct tgid_reply_struct reply;
    int ret = __handle_get_new_tgid(hdr, payload, &reply);
    printf("reply->tgid: %d, reply->ret: %d\n", reply.tgid, reply.ret);
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}

int __handle_create_new_pid_ns(struct mem_header *hdr, void *payload, struct pid_ns_reply_struct *reply) 
{
    struct pid_ns_msg_struct *create_new_pid_ns_req = (struct pid_ns_msg_struct *) payload;
    u16 pid_ns_id = create_new_pid_ns(create_new_pid_ns_req->parent_pid_ns_id);
    reply->pid_ns_id = pid_ns_id;
    reply->ret = 0;
    return 0;
}

int handle_create_new_pid_ns(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct pid_ns_reply_struct reply;
    int ret = __handle_create_new_pid_ns(hdr, payload, &reply);
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}

int __handle_get_tgid_in_ns(struct mem_header *hdr, void *payload, struct get_tgid_ns_reply_struct *reply) 
{
    struct get_tgid_ns_msg_struct *get_tgid_ns_req = (struct get_tgid_ns_msg_struct *) payload;
    struct task_struct *cur_tsk = mn_get_task(get_tgid_ns_req->tgid);
    printf("get_tgid_in_ns: get_tgid_ns_req->tgid: %u, sender_id: %u\n", get_tgid_ns_req->tgid, hdr->sender_id);
    u16 tgid_in_ns = get_tgid_in_ns(get_tgid_ns_req->tgid, get_tgid_ns_req->pid_ns_id);
    reply->tgid_in_ns = tgid_in_ns;
    reply->ret = 0;
    return 0;
}

int handle_get_tgid_in_ns(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct get_tgid_ns_reply_struct reply;
    int ret = __handle_get_tgid_in_ns(hdr, payload, &reply);
    printf("handle_get_tgid_in_ns: ret %d\n", ret);
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}


static int __handle_mmap(struct mem_header *hdr, void *payload, struct mmap_reply_struct *reply)
{
    struct mmap_msg_struct *mmap_req = (struct mmap_msg_struct *)payload;
    struct task_struct *tsk = mn_get_task(mmap_req->tgid);
    unsigned long addr = -ENOMEM;

    memset(reply, 0, sizeof(*reply));
    if (!tsk) {
        reply->ret = -1;
        reply->addr = addr;
        goto out;
    }

    printf("MMAP:: nid: %d, tgid: %d, pid: %d, addr: 0x%lx, flag: 0x%lx, len: %lu, file: %lu\n",
            (int)hdr->sender_id, (int)mmap_req->tgid, (int)mmap_req->pid, mmap_req->addr, mmap_req->vm_flags,
            mmap_req->len, mmap_req->file_id);

    addr = mm_do_mmap(tsk,
            mmap_req->addr, // mmap.argv[0]: addr we're trying to mmap to
            mmap_req->len,      // mmap.argv[1]: how many bytes we're trying to map
            mmap_req->prot,     // mmap.argv[2]: args
            mmap_req->flags,    // mmap.argv[3]: MAP_SHARED, MAP_PRIVATE, etc
            mmap_req->vm_flags, // somehow passed in from mmap handler
            mmap_req->pgoff,    // mmap.argv[5]: offset to read from
            (unsigned long *)mmap_req->file_id, // mmap.argv[4]: file to map
            hdr->sender_id,     // who are we?
            mmap_req->need_cache_entry);

    if (IS_ERR_VALUE(addr)) {
        reply->ret = -1;
    } else {
        addr &= PAGE_MASK;
        reply->ret = 1;
    }

    export_mind_maps(mmap_req->tgid, tsk->mm, (struct mind_map_msg *) reply->maps, MAX_MAPS_IN_REPLY);

    printf("MMAP DONE: nid: %d, tgid: %d, pid: %d, addr: 0x%lx, flag: 0x%lx, len: %lu, file: %lu, ret: %ld\n",
            (int)hdr->sender_id, (int)mmap_req->tgid, (int)mmap_req->pid, addr, mmap_req->vm_flags, 
            mmap_req->len, mmap_req->file_id, reply->ret);
    reply->addr = addr;

out:
    return IS_ERR_VALUE(addr) ? -1 : 0;
}

int handle_mmap(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct mmap_reply_struct reply;
    memset(&reply, 0, sizeof(reply));
    int ret = __handle_mmap(hdr, payload, &reply);
    printf("handle_mmap: reply.ret: %ld\n", reply.ret);
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    printf("handle_mmap: tcp_server_send reply.ret: %ld\n", reply.ret);
    return ret;
}

static int __handle_brk(struct mem_header *hdr, void *payload, struct brk_reply_struct *reply)
{
    struct brk_msg_struct *brk_req = (struct brk_msg_struct *)payload;
    int ret = -1;
    struct task_struct *tsk = mn_get_task(brk_req->tgid);
    unsigned long addr = (unsigned long)NULL;

    if (!tsk) {
        printf("BRK_ERR: couldn't find a running task...\n");
        goto out;
    }

    // unsigned long prev_brk = tsk->mm->brk;
    addr = mn_do_brk(tsk, brk_req->addr, hdr->sender_id);
    if (IS_ERR_VALUE(addr)) {
        printf("BRK_ERR, requested addr[%lx], but got \"error\" [%ld]\n", brk_req->addr, addr);
        addr = (unsigned long)NULL;
        ret = -1;
        goto out;
    }

    ret = 0;
    printf("BRK: Success! Exporting maps.\n");
    export_mind_maps(brk_req->tgid, tsk->mm, (struct mind_map_msg *) reply->maps,
            MAX_MAPS_IN_REPLY);
out:
    reply->ret = ret;
    reply->addr = addr;
    return ret;
}

int handle_brk(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct brk_reply_struct reply;
    memset(&reply, 0, sizeof(reply));
    int ret = __handle_brk(hdr, payload, &reply);
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}

static int __handle_munmap(struct mem_header *hdr, void *payload, struct munmap_reply_struct *reply)
{
    struct munmap_msg_struct *munmap_req = payload;
    int ret = -1;
    struct task_struct *tsk = mn_get_task(munmap_req->tgid);

    printf("MUNMAP_REQ: nid: %d, tgid: %d, pid: %d, addr: 0x%lx, len: %lu\n",
               (int)hdr->sender_id, (int)munmap_req->tgid, (int)munmap_req->pid, munmap_req->addr, munmap_req->len);

    if (tsk && tsk->mm && tsk->ref_cnt[hdr->sender_id] > 0) {
        ret = mn_do_munmap(tsk, tsk->mm, munmap_req->addr, munmap_req->len, munmap_req, hdr->sender_id);

        export_mind_maps(munmap_req->tgid, tsk->mm, (struct mind_map_msg *) reply->maps,
                MAX_MAPS_IN_REPLY);
    } else {
	ret = 0;
    }
    reply->ret = ret;
    printf("MUNMAP: nid: %d, tgid: %d, pid: %d, addr: 0x%lx, len: %lu ret: %d\n",
            (int)hdr->sender_id, (int)munmap_req->tgid, (int)munmap_req->pid, munmap_req->addr, munmap_req->len, ret);
    return ret;
}

int handle_munmap(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct munmap_reply_struct reply;
    memset(&reply, 0, sizeof(reply));
    int ret = __handle_munmap(hdr, payload, &reply);
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}

static int __handle_mremap(struct mem_header *hdr, void *payload, struct mremap_reply_struct *reply)
{
    struct mremap_msg_struct *mremap_req = (struct mremap_msg_struct *)payload;
    int ret = -1;
    unsigned long addr = (unsigned long)NULL;
    struct task_struct *tsk = mn_get_task(mremap_req->tgid);

    if (tsk && tsk->mm)
    {
        addr = mn_do_mremap(tsk, mremap_req->addr, mremap_req->old_len,
                            mremap_req->new_len, mremap_req->flags,
                            mremap_req->new_addr); // return new addr
        if (IS_ERR_VALUE(addr))
        {
            addr = (unsigned long)NULL;
            ret = -1;
        }
        else {
            ret = 0;
            export_mind_maps(mremap_req->tgid, tsk->mm, (struct mind_map_msg *) reply->maps, MAX_MAPS_IN_REPLY);
        }
        // Print out inside function instead of here
        printf("MREMAP: tgid: %d, pid: %d, n_addr: 0x%lx, n_len: %lu, res: %d\n",
               (int)mremap_req->tgid, (int)mremap_req->pid, addr, mremap_req->new_len, ret);
    }
    reply->ret = ret;
    reply->new_addr = addr;
    return ret;
}

int handle_mremap(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct mremap_reply_struct reply;
    memset(&reply, 0, sizeof(reply));
    int ret = __handle_mremap(hdr, payload, &reply);
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}

static int __handle_pfault(struct mem_header *hdr, void *payload, struct fault_reply_struct **reply)
{
    struct fault_msg_struct *fault_req = (struct fault_msg_struct *)payload;
    int ret = -1;
    struct task_struct *tsk = mn_get_task(fault_req->tgid);
    unsigned long data_size = 0;
    unsigned long vm_start = 0;
    unsigned long vm_end = 0;
    unsigned long vm_flags = 0;
    void *data_buf = NULL;

    if (tsk && tsk->mm) // No existing entry
    {
        printf("PgFault: tgid: %d, pid: %d, addr: 0x%lx, data_size: %lu, res: %d\n",
               (int)fault_req->tgid, (int)fault_req->pid, fault_req->address, data_size, ret);
        ret = -1;
    }
    else
    {
        fprintf(stderr, "PgFault: tgid: %d, pid: %d not exist\n", (int)fault_req->tgid, (int)fault_req->pid);
    }
    // TODO: we can make it as one copy instead of two copies:
    //          give reply buf instead of tmp buf
    *reply = malloc(sizeof(struct fault_reply_struct) + data_size);
    if (!(*reply))
    {
        ret = -1;
        goto pfault_release;
    }
    memset(reply, 0, sizeof(struct fault_reply_struct) + data_size);
    (*reply)->ret = ret; // return code or error
    (*reply)->vm_start = vm_start;
    (*reply)->vm_end = vm_end;
    (*reply)->vm_flags = vm_flags;
    (*reply)->data_size = data_size;
    (*reply)->tgid = fault_req->tgid;
    (*reply)->pid = fault_req->pid;
    (*reply)->address = fault_req->address;
    if (data_size > 0)
        memcpy(&((*reply)->data), data_buf, data_size);
    asm volatile("": : :"memory");  // barrier
    ret = 0;
pfault_release:
    if (data_buf)
        free(data_buf);

    return ret;
}

int handle_pfault(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct fault_reply_struct *reply = NULL;
    int ret = -1;
    ret = __handle_pfault(hdr, payload, &reply);

    if (!ret && reply)
    {
        tcp_server_send(sk, id, (const char *)reply, sizeof(*reply) + reply->data_size, MSG_DONTWAIT);
        ret = 0; // Must return 0 if it sent reply
    }

    if (reply)
        free(reply);

    return ret;
}

static int __handle_check(struct mem_header *hdr, void *payload)
{
    struct exec_msg_struct *exec_req =
        (struct exec_msg_struct *)payload;
    int ret = -1;

    ret = mn_check_vma(hdr->sender_id, exec_req->tgid, exec_req->pid_ns_id, exec_req);
    return ret;
}

int handle_check(struct mem_header *hdr, void *payload)
{
    return __handle_check(hdr, payload);
}

/* Variables and functions to handle RDMA multiplexing */
static pthread_spinlock_t node_infos_lock;
// Statically assigned for all nodes based on their IP address
static struct dest_info node_infos[DISAGG_MAX_NODE_CTRL] = {0};
// Dynamically assigned for memory nodes (get the first free slot)
static struct mn_status *mn_status[DISAGG_MAX_NODE_CTRL] = {0};

// Number of nodes that reported their RDMA meta-data (e.g., GID, QP, rkey, PSN)
static int connected_nodes = 0;
static int connected_mem_nodes = 0;

void initialize_node_list(void)
{
    int i;
    // Initialize variables
    for (i = 0; i < DISAGG_MAX_NODE_CTRL; i++)
    {
        node_infos[i].node_id = -1;
    }
    // mn_status is dynamically allocated, and initialized when it is allocated
    // Initialize spinlock
    pthread_spin_init(&node_infos_lock, PTHREAD_PROCESS_PRIVATE);
}

struct dest_info *get_node_info(unsigned int nid)
{
    return &node_infos[nid];
}

// y: Sets its argument to the number of mem nodes.
//    Returns an pointer to an array of `mn_status`.
struct mn_status **get_memory_node_status(int *num_mn)
{
    if (!num_mn)
        return NULL;
    (*num_mn) = connected_mem_nodes;
    return mn_status;
}

void increase_mem_node_count(void)
{
    connected_nodes++;
    connected_mem_nodes++;
}

static int set_memory_node(struct dest_info *node, struct socket *sock)
{
    int i;
    if (!node)
        return -1;

    // find the next available slot
    for (i = 0; i < DISAGG_MAX_NODE_CTRL; i++)
    {
        if (mn_status[i] == NULL)
        {
            mn_status[i] = malloc(sizeof(struct mn_status));
            if (!mn_status[i])
                return -1;
            memset(mn_status[i], 0, sizeof(struct mn_status));

            mn_status[i]->node_id = node->node_id;
            mn_status[i]->node_info = node;
            mn_status[i]->alloc_size = 0;

            pthread_spin_init(&mn_status[i]->alloc_lock, PTHREAD_PROCESS_PRIVATE);
            list_init(&mn_status[i]->alloc_vma_list); // Head node
            node->sk = sock;                            // Assign socket only for memory
            sock->sock_is_done = 1;                     // Switch to controller-centric TCP
            return i;
        }
    }
    return -1;
}

static int check_and_set_memory_node(struct dest_info *node, struct socket *sock)
{
    // check it's valid memory node (e.g., compute node does not have base_addr)
    // TODO (yash): this is a bad heuristic.
    if (!node || !node->base_addr)
        return -1;

    return set_memory_node(node, sock);
}

struct dest_info *ctrl_set_node_info(unsigned int nid, int lid, u8 *mac,
                                     unsigned int *qpn, int psn, char *gid,
                                     u32 lkey, u32 rkey,
                                     u64 addr, u64 size, u64 *ack_buf,
                                     struct socket *sk, bool is_mem_node)
{
    int i;

    pthread_spin_lock(&node_infos_lock);

    node_infos[nid].node_id = nid;
    node_infos[nid].lid = lid;
    memcpy(node_infos[nid].mac, mac, sizeof(u8) * ETH_ALEN);
    for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
    {
        node_infos[nid].qpn[i] = qpn[i];
        node_infos[nid].ack_buf[i] = ack_buf[i];
    }
    node_infos[nid].psn = psn;
    memcpy((void *)&node_infos[nid].gid, (void *)gid, sizeof(DISAGG_RDMA_GID_FORMAT));
    node_infos[nid].base_addr = addr;
    node_infos[nid].size = size;
    node_infos[nid].lkey = lkey;
    node_infos[nid].rkey = rkey;
    node_infos[nid].sk = NULL; // It will be set only for memory nodes

    if (is_mem_node)
         set_memory_node(&node_infos[nid], sk);
    pthread_spin_unlock(&node_infos_lock);

    return get_node_info(nid);
}

static void get_magic_handshake_data(int pair_node_id, struct dest_info *local_info)
{
    int i;
    printf("Generate handshake data for [%d]\n", pair_node_id);
    if (local_info)
    {
        char gid_char[] = RDMA_CTRL_GID;
        local_info->node_id = DISAGG_CONTROLLER_NODE_ID;
        local_info->lid = 0;
        local_info->psn = 0;
        for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
            local_info->qpn[i] = i;

        // Generate fake gid for controller
        memset((void *)local_info->gid, 0, sizeof(local_info->gid));
        for (i = 0; i < (int)(sizeof(DISAGG_RDMA_GID_FORMAT) / 2); i++)
        {
            char one_byte_char[3] = {0};
            memcpy(one_byte_char, &gid_char[2 * i], 2);
            local_info->gid[i] = (unsigned char)(strtol(one_byte_char, NULL, 16) & 0xff);
        }
    }
}

static void gid_to_wire_gid(const char *gid, char *wgid)
{
    int i;
    // gid is pass as char pointer, but it is 16 bytes of raw data
    // i.e., size of gid would be sizeof(DISAGG_RDMA_GID_FORMAT)/2
    // memcpy(tmp_gid, gid, sizeof(DISAGG_RDMA_GID_FORMAT));
    for (i = 0; i < (int)(sizeof(DISAGG_RDMA_GID_FORMAT) / 2); ++i)
        sprintf(&wgid[i * 2], "%02x", gid[i] & 0xff);
    wgid[32] = '\0';
}

static void print_node_info(unsigned int nid)
{
    int i;
    char gid_char[sizeof(DISAGG_RDMA_GID_FORMAT) + 1] = {0};
    gid_to_wire_gid(node_infos[nid].gid, gid_char);
    printf("DMA_FIT_API - node info[%d]: lid: %d, psn: %d, gid: %s\n",
           nid, node_infos[nid].lid, node_infos[nid].psn, gid_char);
    printf("            - qpn list:");
    for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
        printf(" %d", node_infos[nid].qpn[i]);
    printf("\n");
}

char *get_memory_node_ip(int nid)
{
    char *ip_str = malloc(sizeof("123.123.123.123\0"));
    if (!ip_str)
        return NULL;

    sprintf(ip_str, "%u.%u.%u.%u", 10, 10, 10,
            nid - (MAX_NUMBER_COMPUTE_NODE + 1) + DISAGG_MEMORY_NODE_IP_START);
    // nid - start index of memory node + base ip for memory node
    return ip_str;
}

static void sw_add_roce_multiplex_rules(int cpu_id, int mem_id, int is_mem_node)
{
    char cn_ip_str[] = "123.123.123.123\0";
    char mn_ip_str[] = "123.123.123.123\0";
    unsigned int reg_idx = 0;
    struct dest_info *cn = get_node_info(1 + cpu_id);
    struct dest_info *mn = get_node_info(1 + MAX_NUMBER_COMPUTE_NODE + mem_id);
    sprintf(cn_ip_str, "%u.%u.%u.%u", 10, 10, 10, DISAGG_COMPUTE_NODE_IP_START + cpu_id);
    sprintf(mn_ip_str, "%u.%u.%u.%u", 10, 10, 10, DISAGG_MEMORY_NODE_IP_START + mem_id);

    // per CN x MN
    if (cn && cn->node_id >= 0 && mn && mn->node_id >= 0)
    {
        int conn_id;
        unsigned int dummy_reg;
        for (conn_id = 0; conn_id < DISAGG_QP_PER_COMPUTE; conn_id++)
        {
            // CN -> MN (after address translation, so dest ip was set to MN's ip)
            reg_idx = (mem_id * NUM_PARALLEL_CONNECTION);          // base for this mem_id
            reg_idx += (cpu_id * DISAGG_QP_PER_COMPUTE) + conn_id; // offset for this cpu_id and qps
            bfrt_add_roce_req(cn_ip_str, mn_ip_str, conn_id,       // dst_qp == conn_id in CN
                              mn->qpn[(cpu_id * DISAGG_QP_PER_COMPUTE) + conn_id],
                              //   mn->qpn[cpu_id + (conn_id * DISAGG_MAX_COMPUTE_NODE)],
                              mn->rkey, reg_idx); // to MN
            // ACKs from mn - dummy, no changes
            dummy_reg = reg_idx;
            reg_idx = (cpu_id * DISAGG_QP_PER_COMPUTE) + conn_id;
            bfrt_add_roce_ack_dest(reg_idx, mn_ip_str, reg_idx, dummy_reg);
        }
    }

    // per MN
    if (is_mem_node)
    {
        if (mn && mn->node_id >= 0 && cpu_id == 0)
        {
            uint64_t va_base = mem_id * MN_VA_PARTITION_BASE;
            // OR u64 va_base = (u64)mem_id << MN_VA_PARTITION_SHIFT;

            // TODO(yash): log addr translation here
            bfrt_add_addr_trans(va_base, 48 - MN_VA_PARTITION_SHIFT, // first 8 bits of 48 bits
                                mn_ip_str, (uint64_t)mn->base_addr - va_base);
        }
    }
    else
    {
        // per CN
        if (cn && cn->node_id >= 0 && mem_id == 0)
        {
            int conn_id;
            for (conn_id = 0; conn_id < DISAGG_QP_PER_COMPUTE; conn_id++)
            {
                // Normal data QPs and eviction (the last QP)
                if (conn_id < DISAGG_QP_ACK_OFFSET ||
                    (conn_id >= DISAGG_QP_EVICT_OFFSET_START && conn_id <= DISAGG_QP_EVICT_OFFSET_END))
                {
                    // MN -> CN (only once per each CN even if there are multiple MNs)
                    reg_idx = (cpu_id * DISAGG_QP_PER_COMPUTE) + conn_id;
                    pr_rule("pgFault QP[conn: %d]: QP[%d] Dest IP[%s] reg[%u]\n", conn_id, cn->qpn[conn_id], cn_ip_str, reg_idx);
                    // dsp_qp == reg_idx of target CN and conn_id
                    bfrt_add_roce_ack(reg_idx, cn->qpn[conn_id], cn_ip_str, reg_idx); // back to CN
                    // conn_id, ip_str -> reg_idx (-> cn->qpn by the rule above)
                    bfrt_add_sender_qp(conn_id, cn_ip_str, reg_idx);
                    bfrt_add_set_qp_idx(conn_id, cn_ip_str, reg_idx);
                }
                else if (DISAGG_QP_INV_ACK_OFFSET_START <= conn_id && conn_id <= DISAGG_QP_INV_ACK_OFFSET)
                {
                    // RoCE-based invalidation ACK
                    reg_idx = (cpu_id * DISAGG_QP_PER_COMPUTE) + conn_id;
                    // dsp_qp == reg_idx of target CN and conn_id
                    bfrt_add_roce_ack(reg_idx, cn->qpn[conn_id], cn_ip_str, reg_idx); // we may not need this...
                    // Invalidation ack (node id, invalidation buffer id, ...)
                    reg_idx = (cpu_id * DISAGG_QP_NUM_INVAL_BUF) + (conn_id - DISAGG_QP_INV_ACK_OFFSET_START);
                    bfrt_add_egressInvRoute_rule(cpu_id, conn_id - DISAGG_QP_INV_ACK_OFFSET_START,
                                                 cn->qpn[conn_id], cn->rkey,
                                                 cn->ack_buf[conn_id], reg_idx);
                    pr_rule("INV_ACK for ACK[%d]: QP[%d] Dest IP[%s] -> RegIdx[%d] Rkey[%d] BufADDR[0x%lx]\n",
                            conn_id - DISAGG_QP_INV_ACK_OFFSET_START, cn->qpn[conn_id], cn_ip_str, cpu_id,
                            cn->rkey, cn->ack_buf[conn_id]);
                }
                else if (conn_id >= DISAGG_QP_DUMMY_OFFSET_START && conn_id <= DISAGG_QP_DUMMY_OFFSET_END)
                {
                    reg_idx = (cpu_id * DISAGG_QP_PER_COMPUTE) + conn_id;
                    // QP for dummy roce ack for NACK
                    // Page fault conn -> dummy conn
                    bfrt_add_roce_dummy_ack(conn_id - DISAGG_QP_DUMMY_OFFSET_START, cn_ip_str,
                                            conn_id, cn->ack_buf[conn_id]); // forward to CN (itself)
                    // Dummy ack request
                    bfrt_add_roce_req(cn_ip_str, cn_ip_str, conn_id,
                                      cn->qpn[conn_id], cn->rkey, reg_idx);
                    // Dummy ack roce ack (back to page fault QP)
                    bfrt_add_roce_ack_dest(conn_id, cn_ip_str,
                                           reg_idx - DISAGG_QP_DUMMY_OFFSET_START, reg_idx);
                }
                else
                {
                    // QP for ACK
                    reg_idx = (cpu_id * (DISAGG_QP_PER_COMPUTE)) + (conn_id - DISAGG_QP_ACK_OFFSET);
                    pr_rule("Conn for ACK[%d]: QP[%d] Dest IP[%s] -> RegIdx[%d] Rkey[%d] BufADDR[0x%lx]\n",
                            conn_id, cn->qpn[conn_id], cn_ip_str, reg_idx, cn->rkey, cn->ack_buf[conn_id]);
                    bfrt_add_ack_trans(cn_ip_str, conn_id - DISAGG_QP_ACK_OFFSET,
                                       cn->qpn[conn_id], cn->rkey, cn->ack_buf[conn_id], reg_idx);
                }
            }
        }
    }
}

// extern functions from config
extern unsigned int get_compute_start_ip_last_digits(void);
extern unsigned int get_memory_start_ip_last_digits(void);

// int get_nid_from_ip_str(char *ip_addr, int is_mem_node)
// {
//     unsigned int ip_addr_num[4];
//     int nid;

//     // == Assign node based on IP ==
//     // Controller:      0
//     // Computing node:  1 to MAX_NUMBER_COMPUTE_NODE
//     // Memory node:     (MAX_NUMBER_COMPUTE_NODE + 1) to (DISAGG_MAX_NODE_CTRL - 1)

//     sscanf(ip_addr, "%u.%u.%u.%u", &ip_addr_num[0], &ip_addr_num[1], &ip_addr_num[2], &ip_addr_num[3]);
//     if (is_mem_node)
//     {

//         nid = ip_addr_num[3] - get_memory_start_ip_last_digits() + MAX_NUMBER_COMPUTE_NODE + 1;
//     }
//     else
//     {
//         nid = ip_addr_num[3] - get_compute_start_ip_last_digits() + 1;
//     }
//     return nid;
// }

int get_nid_from_ip_array(u8 *ip_addr, int is_mem_node)
{
    int nid;

    // == Assign node based on IP ==
    // Controller:      0
    // Computing node:  1 to MAX_NUMBER_COMPUTE_NODE
    // Memory node:     (MAX_NUMBER_COMPUTE_NODE + 1) to (DISAGG_MAX_NODE_CTRL - 1)
    // printf("Given IP: %u.%u.%u.%u\n", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
    // printf("Start comp: %u\n", get_compute_start_ip_last_digits());
    // printf("Start mem: %u\n", get_memory_start_ip_last_digits());

    return 1;

    // yash: We only should run this function once. For the client node. Never again.

    // if (is_mem_node)
    // {
    //
    //     nid = ip_addr[3] - get_memory_start_ip_last_digits() + MAX_NUMBER_COMPUTE_NODE + 1;
    // }
    // else
    // {
    //     nid = ip_addr[3] - get_compute_start_ip_last_digits() + 1;
    // }
    // return nid;
}

static void print_rdma_msg_struct(struct rdma_msg_struct *msg)
{
    printf("rdma_msg_struct:\n"
            "\tret=%d\n"
            "\tnode_id=%d\n"
            "\tnode_type=%d\n"
            "\tnode_id_res=%d\n"
            "\taddr=%lu\n"
            "\tsize=%lu\n"
            "\trkey=%d\n"
            "\tlkey=%d\n"
            "\tqpn=NOT SHOWN\n"
            "\tpsn=%d\n"
            "\tip_address=%d:%d:%d:%d\n"
            "\tbase_addr=%lx\n"
            "\tkshmem_va_start=%lx\n",
            msg->ret, msg->node_id, msg->node_type,
            msg->node_id_res, msg->addr, msg->size, msg->rkey, msg->lkey,
            msg->psn,
            msg->ip_address[0], msg->ip_address[1], msg->ip_address[2], msg->ip_address[3],
            msg->base_addr, msg->kshmem_va_start);
}

// y: When a cluster node initializes itself, it sends us this message.
int handle_client_rdma_init(
        struct mem_header *hdr, void *payload, struct
        socket *sk, int id, char *ip_addr,
        int *new_nid) // This variable is an _output_ of the function.
{
    struct rdma_msg_struct *rdma_req = (struct rdma_msg_struct *)payload;
    struct rdma_msg_struct *reply;
    struct dest_info local_info;
    int ret = -1;
    int nid;
    char gid_char[sizeof(DISAGG_RDMA_GID_FORMAT) + 1] = {0};
    int duplicated_node = 0;
    int i;
    (void)hdr;
    bool is_mem_node = rdma_req->node_type == DISAGG_RDMA_MEMORY_TYPE;
    char cluster_ip_str[] = "123.123.123.123\0";
    u32 request_qpn[DISAGG_MAX_COMPUTE_NODE * DISAGG_QP_PER_COMPUTE];
    u64 request_ack_buf[DISAGG_QP_PER_COMPUTE];

    printf("Receiving RDMA init message!\n");
    print_rdma_msg_struct(rdma_req);

    // We create a local copy of array to avoid "taking address of packed member => unaligned
    // pointer" warning.
    for (int i = 0; i < DISAGG_MAX_COMPUTE_NODE * DISAGG_QP_PER_COMPUTE; i++)
        request_qpn[i] = rdma_req->qpn[i];
    for (int i = 0; i < DISAGG_QP_PER_COMPUTE; i++)
         request_ack_buf[i] = rdma_req->qpn[i];


    // PHASE 1: Assign a NID slot for the new node, based on its IP address
    //          (NID = offset from base IP address of MN/CNs).

    nid = get_nid_from_ip_array(rdma_req->ip_address, is_mem_node);
    if (new_nid)
        *new_nid = nid;

    // handle request
    if (nid == DISAGG_CONTROLLER_NODE_ID)
    {
        error_and_exit("Cannot use controller's id", __func__, __LINE__);
    }
    else if (nid < DISAGG_MAX_NODE_CTRL && nid >= 0)
    {
        if (node_infos[nid].node_id >= 0)
        {
            print_node_info(nid);
            printf("RDMA_INIT: ERROR: reuse the existing node id: %d\n", nid);
            duplicated_node = 1;
        }
        ctrl_set_node_info(nid, rdma_req->lid, rdma_req->mac, request_qpn, rdma_req->psn,
                            rdma_req->gid, rdma_req->lkey, rdma_req->rkey,
                            rdma_req->addr, rdma_req->size, request_ack_buf, sk,
                            is_mem_node); // Copy the gid assuming same endian
        if (!is_mem_node)
        {
            // kernel's shared memory
            kshmem_set_va_start(rdma_req->kshmem_va_start, nid);
        }
        ret = 0;
    }
    else
    {
        printf("Unexpected IP address: %s | nid: %d\n", ip_addr, nid);
        error_and_exit("IP address is out of range", __func__, __LINE__);
    }

    // Create reply
    reply = malloc(sizeof(*reply));
    if (!reply)
    {
        ret = -1;
        goto rdma_init_release;
    }
    get_magic_handshake_data(nid, &local_info); // Static data for controller
    reply->ret = ret;
    reply->node_id = local_info.node_id;
    reply->node_id_res = nid; // New id for disaggregated nodes
    reply->lid = local_info.lid;
    reply->lkey = 0; // Key are only stored in switch
    reply->rkey = 0;

    if (is_mem_node) // If memory node
    {
        reply->psn = 0; // just initial value
        for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
            reply->qpn[i] = local_info.qpn[i];
        sprintf(cluster_ip_str, "%u.%u.%u.%u", 10, 10, 10, DISAGG_MEMORY_NODE_IP_START + nid - (MAX_NUMBER_COMPUTE_NODE + 1));
    }
    else
    {
        reply->psn = 0;
        for (i = 0; i < DISAGG_QP_PER_COMPUTE; i++)
            reply->qpn[i] = local_info.qpn[i];
        sprintf(cluster_ip_str, "%u.%u.%u.%u", 10, 10, 10, DISAGG_COMPUTE_NODE_IP_START + nid - 1);
    }

    memset(reply->gid, 0, sizeof(reply->gid));
    memcpy(reply->gid, (void *)local_info.gid, min(sizeof(reply->gid), sizeof(local_info.gid)));
    gid_to_wire_gid(local_info.gid, gid_char);

    printf("RDMA_INIT: handshake with node[%d/%u]\n **local - lid: %d, psn: %d, qpn[0]: %d\n\t\
        base_addr (test): 0x%lx, gid: %s\n",
           nid, reply->node_id_res, reply->lid, reply->psn, reply->qpn[0], (unsigned long)reply->base_addr, gid_char);
    if (!ret)
    {
        memset(gid_char, 0, sizeof(gid_char));
        gid_to_wire_gid(rdma_req->gid, gid_char);
        printf(" **remote - lid: %d, psn: %d, qpn[0]: %d, lkey: 0x%x, rkey: 0x%x\n\t\
        addr: 0x%lx, gid: %s\n",
               rdma_req->lid, rdma_req->psn, rdma_req->qpn[0], rdma_req->lkey, rdma_req->rkey,
               (unsigned long)rdma_req->addr, gid_char);

        // IP based static rules
        if (!duplicated_node)
        {
            if (is_mem_node) // Memory node
            {
                int mem_id = (nid - MAX_NUMBER_COMPUTE_NODE - 1);
                int cpu_id;
                for (cpu_id = 0; cpu_id < MAX_NUMBER_COMPUTE_NODE; cpu_id++)
                {
                    sw_add_roce_multiplex_rules(cpu_id, mem_id, 1);
                }
            }
            else
            {
                int cpu_id = (nid - 1);
                int mem_id;
                for (mem_id = 0; mem_id < MAX_NUMBER_MEMORY_NODE; mem_id++)
                {
                    sw_add_roce_multiplex_rules(cpu_id, mem_id, 0);
                }
                bfrt_add_cachesharer(cluster_ip_str, 1 << cpu_id);
                bfrt_add_eg_cachesharer(cluster_ip_str, 1 << cpu_id);
            }
        }
        // Else if duplicated, existing entry must be erased first
        // (maybe not needed if the base memory is not changed and have same memory?)
    }

    printf("Sending reply to RDMA init message!\n");
    print_rdma_msg_struct(reply);

    tcp_server_send(sk, id, (const char *)reply, sizeof(*reply), MSG_DONTWAIT);
    ret = 0; // Must return 0 if it sent reply

    if (!duplicated_node)
    {
        connected_nodes++;
        if (rdma_req->addr)
            connected_mem_nodes++;
    }

rdma_init_release:
    if (reply)
        free(reply);
    return ret;
}

void error_and_exit(const char *err_msg, const char *ftn, long line)
{
    fprintf(stderr, "Error: %s at %s: %ld\n", err_msg, ftn, line);
    exit(-1);
}

/* vim: set tw=99 */
