/*
 * Header file of fork and disaggregated fork functions
 */
#include <disagg/network_disagg.h>
#include <disagg/exit_disagg.h>
#include <disagg/cnthread_disagg.h>
#include <disagg/config.h>
#include <disagg/print_disagg.h>
#include <linux/exec.h>

static int send_exit_mm(struct task_struct *tsk, unsigned long clone_flags)
{
    struct exit_msg_struct payload;
    struct exit_reply_struct *reply;
    int ret;

    reply = kmalloc(sizeof(struct exit_reply_struct), GFP_KERNEL);
    if (!reply)
         return -ENOMEM;

    payload.pid = tsk->pid;
    payload.tgid = tsk->tgid;
    payload.pid_ns_id = tsk->pid_ns_id; // TODO: Ziming
    payload.flag = clone_flags;

    pr_syscall("send_exit_mm send_msg_to_control DISAGG_EXIT, tgid: %d, pid: %d\n", tsk->tgid, tsk->pid);

    ret = send_msg_to_control(DISAGG_EXIT, &payload, sizeof(payload), 
            reply, sizeof(*reply));

    // ret = send_msg_to_memory_rdma(DISAGG_EXIT, &payload, sizeof(payload), 
    //                         reply, sizeof(*reply));
    pr_syscall("EXIT - Data from CTRL [%d]: ret: %d [0x%llx]\n",
            ret, reply->ret, *(long long unsigned*)(reply));

    if (ret >= 0)
         ret = reply->ret;   // set error

    kfree(reply);
    return ret;
}

/*
 * Currently this function forward request to memory node 
 * while it just use the local functions
 */
int disagg_exit(struct task_struct *tsk)
{
    int err;
    int cnt __maybe_unused = decrement_test_program_thread_cnt(tsk->tgid);
    unsigned long exit_flag = EXIT_FIRST_TRY;

    if (tsk->is_remote)
    {
        pr_syscall("EXIT: cnt[%d], tgid: %d\n", cnt, tsk->tgid);

retry_send_exit:
        err = send_exit_mm(tsk, exit_flag);
        if (err == -ERR_DISAGG_REMOTE_WAIT_EXIT) {
            pr_info("remote tgroup wait exit: tgid: %d, pid: %d\n", tsk->tgid, tsk->pid);
            ssleep(MIND_EXIT_RETRY_IN_SEC);
            exit_flag = EXIT_RETRY;
            goto retry_send_exit;
        }
    } else {
        err  = send_exit_mm(tsk, 0);
    }

    // Dealloc used RDMA QPs, we don't need them anymore.
    if (tsk->is_remote) {
        BUG_ON(unlikely(mind_rdma_put_qp_handle_fn == NULL));
        mind_rdma_put_qp_handle_fn(tsk->qp_handle);
    }

    if (err == -ERR_DISAGG_EXIT_NO_TASK) {
		printk(KERN_ERR "EXIT: Cannot find tgid %d (%d)\n", 
				tsk->tgid, err);
        err = 0; // Already cleared by other tasks
        // current->is_remote = false;
    }
    else if (err < 0){
		printk(KERN_ERR "EXIT: Cannot send exit_mm to memory (%d), %s\n", 
				err, tsk->comm);
    }else{
		err = 0;
	} // Now we are free to free all the local resources

    //TODO clear req buf if cnt == 0?
    // clear cache
    // disagg_clear_req_buffer(tsk);

    // return ASID
    BUG_ON(!tsk->mm->is_remote_mm);
    put_disagg_asid(tsk->mm->remote_asid);

    pr_info("EXIT: tgid %d\n", tsk->tgid);
    return err;
}

int disagg_exit_for_test(struct task_struct *tsk)
{
    return disagg_exit(tsk);
}
EXPORT_SYMBOL(disagg_exit_for_test); // for unit test in RoceModule
