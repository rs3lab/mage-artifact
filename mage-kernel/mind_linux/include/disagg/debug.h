#ifndef __DEBUG_DISAGGREGATION_H__
#define __DEBUG_DISAGGREGATION_H__

// #define MIND_ZERO_REMOVED_FRAME

// Debugging flag for verifying incoming/outgoing data
// WARNING: it only supports processes running on a single compute blade!
// #define MIND_VERIFY_PAGE_CHKSUM  // must be enabled if one of the following lines is enabled
// #define DISAGG_ENABLE_MEMORY_CHECKSUM_VERIFICATION
// #define DISAGG_ENABLE_MEMORY_CHECKSUM_PRINTING

// #define DISAGG_DEBUG_TGID_CONCURRNT_PGFAULT 0x1
// #define DISAGG_DEBUG_TGID_PGFAULT_PROFILE 0x2

#ifdef DISAGG_DEBUG_TGID_PGFAULT_PROFILE
struct pf_profile_node {
    unsigned int tgid;
    unsigned long addr;
    struct hlist_node hnode;
    struct list_head node;
    atomic_long_t cnt;
    unsigned int latest_cpu_id;
};

void add_pf_profile_counter(unsigned int tgid, unsigned long addr, int cpu_id);
void increase_pf_profile_counter(unsigned int tgid, unsigned long addr, int cpu_id);
void print_pf_profile(void);
void print_pf_profile_entry(unsigned int tgid, unsigned long addr, int cur_cpu);
#endif

#define _mind_debug_str_size 384
#define _mind_debug_stack_str_size 8192

struct err_msg_struct {
	unsigned int pid;
	unsigned int tgid;
	char debug_str[_mind_debug_str_size];
	unsigned long reserved[8];
} __packed;

struct err_stack_msg_struct {
	unsigned int pid;
	unsigned int tgid;
	char debug_str[_mind_debug_stack_str_size];
	unsigned long reserved[8];
} __packed;

struct err_reply_struct {
	int			ret;		// response
} __packed;

#endif  // __DEBUG_DISAGGREGATION_H__
