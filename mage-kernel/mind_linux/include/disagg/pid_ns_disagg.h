#ifndef __PID_NS_DISAGGREGATION_H__
#define __PID_NS_DISAGGREGATION_H__

// THIS FILE IS NEVER USED...IT'S JUST NEEDED FOR THE FRONTEND TO BUILD. 
// TODO DELETE IT LATER. 

#include <linux/types.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define NO_ERR 0
#define NO_PID 1

struct tgid_msg_struct
{
	int pid_ns_id;    // pid_ns_id
	u32 sender_id;    // id of the sender node
	int option;
} __packed;

struct tgid_reply_struct
{
	int ret;    // error code
	pid_t tgid; // Returned new tgid
} __packed;

struct pid_ns_msg_struct
{
	int option;
	u32 parent_pid_ns_id;
	int ret;
} __packed;

struct pid_ns_reply_struct
{
	int ret;
	int pid_ns_id;
} __packed;

struct get_tgid_ns_msg_struct 
{
	int option;
	u16 tgid;
	u16 pid_ns_id;
} __packed;

struct get_tgid_ns_reply_struct 
{
	int ret;
	u16 tgid_in_ns;
} __packed;

#endif /* __PID_NS_DISAGGREGATION_H__ */

/* vim: set ts=8 sts=8 sw=8 noexpandtab */
