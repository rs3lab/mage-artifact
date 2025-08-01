#ifndef __MMAP_DISAGGREGATION_H__
#define __MMAP_DISAGGREGATION_H__

// NOTE: BEFORE INCLUDING THIS FILE, ALWAYS INCLUDE <disagg/cnmap_disagg.h>.
//
// Unfortunately we can't include cnmap_disagg here because this file is
// also sourced by the switch controller => recursive includes fail :(

#include <linux/sched.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define VM_CACHE_OWNER 0x04000000

struct mmap_msg_struct {
	u16	pid;        // y: PID of requesting process.
	u16	tgid;       // y: TGID of requesting process.
	u32	pid_ns_id;  // y: PID namespace ID. Think containers.
	u32	need_cache_entry;
	unsigned long addr; // y: Virtual addr we're writing to
	unsigned long len;  // y: How many bytes we're mapping
	unsigned long prot; // y: Permissions
	unsigned long flags;
	unsigned long vm_flags;
	unsigned long pgoff;
	// y: if this is a read-only file mapping, `file_id` contains the ID
	// of that file. Writable file mappings have this set to zero; MIND
	// pushes the data to far memory initially, so it don't have to
	// reference the file when dealing w/ MMAPs.
	unsigned long file_id; 
} __packed;

struct mmap_reply_struct {
	// y: Used as the return code of the mmap syscall.
	unsigned long   addr; 
	// reply->ret = 0: success, cacheline populated, 1: success, cacheline not populated
	long	    ret;
	// y: Used for non-virtualized RDMA between CN and MN only.
	//    There may be a code warning below, due to the "include" issues
	//    mentioned at the top of this file.
	struct mind_map_msg maps[MAX_MAPS_IN_REPLY];
} __packed;


struct brk_msg_struct {
	u32	pid;
	u32	tgid;
	u32 pid_ns_id;
	unsigned long addr;
} __packed;

struct brk_reply_struct {
	unsigned long   addr;
	int             ret;
	struct mind_map_msg maps[MAX_MAPS_IN_REPLY];
} __packed;

struct munmap_msg_struct {
	u32	pid;
	u32	tgid;
	u32 pid_ns_id;
	unsigned long addr;
    unsigned long len;
} __packed;

struct munmap_reply_struct {
	int                ret;	// error code
	struct mind_map_msg    maps[MAX_MAPS_IN_REPLY];
} __packed;

struct mremap_msg_struct {
	u32	pid;
	u32	tgid;
	u32 pid_ns_id;
	unsigned long addr;
    unsigned long old_len;
    unsigned long new_len;
    unsigned long flags;
    unsigned long new_addr;
} __packed;

struct mremap_reply_struct {
	int			  ret;		// error code
    unsigned long new_addr;
    struct mind_map_msg maps[MAX_MAPS_IN_REPLY];
} __packed;
#endif /* __MMAP_DISAGGREGATION_H__ */
