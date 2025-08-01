#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <disagg/cnmap_disagg.h>
#include <linux/fork.h>
#include <linux/sched/task.h>
#include <disagg/fork_disagg.h>
#include <disagg/launch_disagg.h>
#include <disagg/print_disagg.h>
#include <disagg/network_disagg.h>
#include <disagg/network_fit_disagg.h>
#include <disagg/exec_disagg.h>
#include <disagg/config.h>
#include <asm/proto.h>
#include <asm/prctl.h>
#include <asm/switch_to.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <disagg/mmap_disagg.h>
#include <disagg/cnthread_disagg.h>
#include <disagg/fault_disagg.h>
#include <linux/oom.h>
#include <linux/hashtable.h>
#include <linux/mmu_notifier.h>
#include <asm/tlb.h>
#include <asm/pgtable_types.h>
#include <asm/pgtable.h>
#include <asm/page_types.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

#define IN_KERNEL_SPACE // This is not a good practice but for convenience sake.
#include "../tcp_daemon.hpp"

char *send_buffer = NULL;
int send_buffer_len = 0;
char *recv_buffer = NULL;
int recv_buffer_len = 0;
int kernel_buffer_received = 0;
// spinlock_t kernel_buffer_lock;

void reset_send_buffer_and_len(void) {
    send_buffer = NULL;
    send_buffer_len = 0;
}

void reset_recv_buffer_and_len(void) {
    recv_buffer = NULL;
    recv_buffer_len = 0;
}

/* This should be protected by atomic ongoing_payload */
int register_send_recv_buffer(char *from, int from_length, char *to, int to_length) {

    if (kernel_buffer_received == 1)
        BUG();

    if (!from || from_length < 0 || !to || to_length < 0)
        BUG();
        
    send_buffer = from;
    send_buffer_len = from_length;
    recv_buffer = to;
    recv_buffer_len = to_length;
    return from_length;
}

int read_from_buffer(char *to, int to_length) {

    int ret_length = 0;
    // spin_lock(&kernel_buffer_lock);

    if (to != recv_buffer || to_length != recv_buffer_len)
        BUG();

    // y: This should be a proper condvar! One compiler optimization and this
    while (!kernel_buffer_received) {
        if (signal_pending(current))
        {
            __set_current_state(TASK_RUNNING);
            return -1;
        }
        // y: If this is converted to a `usleep_range`, the system
        //    complains about sleeping while under `TASK_UNINTERRUPTIBLE`.
        //    Ask yash to see `run-v7-atomic-sleeps` if you want more
        //    details.
        udelay(10);
    }
    ret_length = kernel_buffer_received;
    kernel_buffer_received = 0;
    reset_recv_buffer_and_len();
    // spin_unlock(&kernel_buffer_lock);
    return ret_length;
}

/*
 * Syscall 653: Get send_buffer_len and recv_buffer_len
 * @input: from_len - pointer to send_len, to_len - pointer to recv_len
 * @return: 0
 */
SYSCALL_DEFINE2(get_buffer_len, int *, from_len, int *, to_len) {

    while (!send_buffer_len) {
        if (signal_pending(current))
        {
            __set_current_state(TASK_RUNNING);
            return -1;
        }
        usleep_range(10, 10);
    }
    copy_to_user(from_len, &send_buffer_len, sizeof(int));
    copy_to_user(to_len, &recv_buffer_len, sizeof(int));
    return 0;
}

/*
 * Syscall 651: Copy content from kernel buffer to user buffer
 * @input: buffer - user space buffer, buffer_len - user buffer len
 * @return: 0
 */
SYSCALL_DEFINE2(copy_msg_to_user_buffer, char *, buffer, int, buffer_len) {

    if (buffer_len != send_buffer_len || !buffer || !send_buffer) {
        pr_err("%s: buffer_len [%d], send_buffer_len [%d]", __func__, buffer_len, send_buffer_len);
        BUG();
    }

    copy_to_user(buffer, send_buffer, send_buffer_len);
    return 0;
}

/*
 * Syscall 652: TCP Daemon write message to kernel buffer
 * @input: buffer - user space buffer, buffer_len - user buffer len, ret_len: return len
 * @return: 0
 */
SYSCALL_DEFINE3(write_msg_to_kernel, char *, buffer, int, buffer_len, int, ret_len) {

    if (buffer_len != recv_buffer_len) {
        pr_err("%s: buffer_len [%d], recv_buffer_len [%d]", __func__, buffer_len, recv_buffer_len);
        BUG();
    }

    if (!recv_buffer)
        BUG();

    copy_from_user(recv_buffer, buffer, recv_buffer_len);

    barrier();
    reset_send_buffer_and_len();
    kernel_buffer_received = ret_len;
    return 0;
}
