#ifndef __PID_DISAGGREGATION_H__
#define __PID_DISAGGREGATION_H__

// THIS FILE IS NEVER USED...IT'S JUST NEEDED FOR THE FRONTEND TO BUILD. 
// TODO DELETE IT LATER. 

#include <linux/types.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define NO_ERR 0
#define NO_PID 1

struct pid_msg_struct
{
    int option;
} __packed;

struct pid_reply_struct
{
    int ret;    // error code
    pid_t tgid; // Returned new tgid
} __packed;

#endif /* __PID_DISAGGREGATION_H__ */
