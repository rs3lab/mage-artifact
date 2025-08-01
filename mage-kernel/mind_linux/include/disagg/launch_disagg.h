#ifndef __LAUNCH_DISAGGREGATION_H__
#define __LAUNCH_DISAGGREGATION_H__

#include <linux/sched.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define LAUNCH_ADDRESS_LENGTH 256
struct launch_task_struct {
	char addr[LAUNCH_ADDRESS_LENGTH];
} __attribute__((packed));

#ifndef BF_CONTROLLER
struct launch_req_struct {
    struct launch_task_struct *launch_msg;
    struct list_head node;
};
#endif

int add_one_launch_req(struct launch_task_struct *payload, int alloc);

enum {
    DISAGG_LAUNCH_NO_MSG = 0,
    DISAGG_LAUNCH_FORK_MSG = 1,
    DISAGG_LAUNCH_LAUNCH_MSG = 2,
};

#endif

