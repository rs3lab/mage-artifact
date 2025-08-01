#ifndef __EXEC_DISAGGREGATION_H__
#define __EXEC_DISAGGREGATION_H__

// NOTE: BEFORE INCLUDING THIS FILE, ALWAYS INCLUDE <disagg/cnmap_disagg.h>.
//
// Unfortunately we can't include cnmap_disagg here because this file is
// also sourced by the switch controller => recursive includes fail :(

#ifndef BF_CONTROLLER
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/types.h>
#endif

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define ERR_DISAGG_EXEC_TIMEOUT 1

/* vma info will be contain range, flags, permissions, etc. */
struct exec_vmainfo {
	unsigned long	vm_start;
	unsigned long	vm_end;
	unsigned long	vm_flags;
	unsigned long	vm_pgoff;	//for stack
	unsigned long	rb_substree_gap;
	unsigned long	vm_page_prot;
	unsigned long	file_id;
} __packed;

struct exec_msg_struct {
	u32	pid;
	u32	tgid;
	u32 pid_ns_id;
	// __u32	parent_tgid;
	// __u32	clone_flags;
	char	comm[TASK_COMM_LEN];

	// mm related params
	unsigned long hiwater_rss;	/* High-watermark of RSS usage */
	unsigned long hiwater_vm;	/* High-water virtual memory usage */

	unsigned long total_vm;		/* Total pages mapped */
	unsigned long locked_vm;	/* Pages that have PG_mlocked set */
	unsigned long pinned_vm;	/* Refcount permanently increased */
	unsigned long data_vm;		/* VM_WRITE & ~VM_SHARED & ~VM_STACK */
	unsigned long exec_vm;		/* VM_EXEC & ~VM_WRITE & ~VM_STACK */
	unsigned long stack_vm;		/* VM_STACK */
	unsigned long def_flags;
	unsigned long start_code, end_code, start_data, end_data;
	unsigned long start_brk, brk, start_stack;
	unsigned long arg_start, arg_end, env_start, env_end;
	unsigned long mmap_base, mmap_legacy_base;

	// they should be the last variables - dynamic length
	u32		num_vma;
	struct exec_vmainfo	vmainfos;	//[DEFAULT_MAX_MAP_COUNT];
} __packed;

struct exec_reply_struct {
	int			ret;		// error code
	u32			vma_count;	// number of copied vma
    struct mind_map_msg maps[MAX_MAPS_IN_REPLY];
} __packed;

#define CN_SWITCH_REG_SYNC_NONE	0
#define CN_SWITCH_REG_SYNC_PUSH 1
#define CN_SWITCH_REG_SYNC_PULL 2
#define CN_SWITCH_RST_ON_UPDATE 7	// reset on-going flag of cache directory entry

enum{
	CN_ONLY_DATA = 0,
	CN_OTHER_PAGE = 2,
};

#ifdef CONFIG_COMPUTE_NODE
int copy_vma_to_mn(struct task_struct *tsk, u32 msg_type);
int cn_copy_vma_data_to_mn(struct task_struct *tsk, struct vm_area_struct *vma, 
		unsigned long start_addr, unsigned long end_addr, off_t off_in_vma);
int cn_copy_page_data_to_mn(u16 tgid, struct mm_struct *mm, unsigned long addr, pte_t *pte,
							int is_target_data, u32 req_qp, void *dma_addr);
//int cn_copy_page_data_to_mn_from_file(u16 tgid, struct vm_area_struct *vma, unsigned long addr);
int count_vm_field(struct task_struct *tsk);

#endif /* CONFIG_COMPUTE_NODE */
#endif /* __EXEC_DISAGGREGATION_H__ */
