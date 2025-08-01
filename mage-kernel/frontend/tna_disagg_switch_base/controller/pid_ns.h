#ifndef __PID_NS_H__
#define __PID_NS_H__
#include "types.h"

extern u16 max_pid_ns_id; // FIRST FEW PID_NS_ID is reserved for kernel stuff

#define START_PID_NS_ID 0
#define START_TGID_NS 1
/** Initialize the pid namespace id to tgid mapping */
void pid_ns_init();

/** Get new tgid for the pid namespace */
u16 get_new_tgid(u16 sender_id, u16 ns_id);

/** Create new pid ns and return the id of the new ns */
u16 create_new_pid_ns(u16 parent_pid_ns_id);

u32 generate_nstgid(u16 tgid, u16 pid_ns_id);

int put_tgid_to_ns_table(u16 tgid, u16 ns_id);
int remove_tgid_from_ns_table(u16 tgid, u16 ns_id);
int contains_tgid_ns(u16 tgid, u16 ns_id);
u16 get_tgid_in_ns(u16 tgid, u16 pid_ns_id);

struct pid_namespace {
	unsigned int level;
	struct pid_namespace *parent;
	u16 max_nr;
};

struct upid {
	u16 nr;
	struct pid_namespace *pid_ns;
};

#endif