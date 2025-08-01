#include <linux/kernel.h>
#include <linux/syscalls.h>

//syscall 549
SYSCALL_DEFINE2(launch_task_thread, int, tid, char *, buffer) {
    BUG();
    return 0;
}

// syscall: 550
SYSCALL_DEFINE1(initialize_remote_thread, int, tid) {
    BUG();
    return 0;
}

// syscall: 548
SYSCALL_DEFINE2(disagg_handle_remote_thread, int, tid, int, num_container_threads) {
    BUG();
    return 0;
}
