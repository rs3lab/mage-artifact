#ifndef IN_KERNEL_SPACE
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <linux/hw_breakpoint.h>
#include <sys/mman.h>
#include <sys/time.h>

#define NOT_IMPORT_LINUX
#include "../../../include/disagg/network_fit_disagg.h"
#include "../../../include/disagg/cnthread_disagg.h"

#define TCP_DAEMON
#include "../../../include/disagg/network_disagg.h"

/* header for messaging */
#define MEMORY_HEADER_ALIGNMENT 8
#define DISAGG_NET_CTRL_POLLING_SKIP_COUNTER 10000	// skip counter for control plane
#define ERR_DISAGG_NET_TIMEOUT          1
#define DISAGG_CONN_USAGE_REMOTE_FORK        62

struct mem_header {
    uint32_t opcode;    // type of payload / message
    uint32_t sender_id; // id of the sender node
    uint32_t size;      // size of the payload (include this header)
} __attribute__((aligned(MEMORY_HEADER_ALIGNMENT)));

int get_local_node_id(void) {
    return 1;
}


static int pin_to_core(int core_id) {
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (core_id < 0 || core_id >= num_cores) {
        printf("pin to core[%d] failed, total cores[%d]\n", core_id, num_cores);
        return -1;
    }
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_t current_thread = pthread_self();
    int err = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
    usleep(10000);
    return err;
}

#endif

#define SWITCH_IP "192.168.122.1"
#define SWITCH_PORT 38001
#define ERR_DISAGG_NET_FAILED_TX        5
#define COPY_MSG_USER_BUFFER_SYSCALL 651
#define WRITE_MSG_TO_KERNEL_SYSCALL 652
#define GET_BUFFER_LEN_SYSCALL 653
// #define KERNEL_BUFFER_SIZE 10000
