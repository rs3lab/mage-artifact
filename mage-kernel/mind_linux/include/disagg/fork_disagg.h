#ifndef __FORK_DISAGGREGATION_H__
#define __FORK_DISAGGREGATION_H__

#include <linux/sched.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define ERR_DISAGG_FORK_TIMEOUT 101
#define ERR_DISAGG_FORK_NO_PREV 102
#define ERR_DISAGG_FORK_THREAD 	103
#define ERR_DISAGG_FORK_REMOTE_THREAD 	104

// FIXME all these configuration should be shared with the kernel code instead of duplication here
#define GDT_ENTRY_TLS_ENTRIES 3
#define MAX_FILE_PATH_NAME 128

/* 8 byte segment descriptor */
struct user_desc_struct {
	u16	limit0;
	u16	base0;
	u16	base1: 8, type: 4, s: 1, dpl: 2, p: 1;
	u16	limit1: 4, avl: 1, l: 1, d: 1, g: 1, base2: 8;
} __attribute__((packed));

struct user_pt_regs {
	/*
	 * C ABI says these regs are callee-preserved. They aren't saved on kernel entry
	 * unless syscall needs a complete, fully filled "struct pt_regs".
	 */
	unsigned long r15;
	unsigned long r14;
	unsigned long r13;
	unsigned long r12;
	unsigned long bp;
	unsigned long bx;
	/* These regs are callee-clobbered. Always saved on kernel entry. */
	unsigned long r11;
	unsigned long r10;
	unsigned long r9;
	unsigned long r8;
	unsigned long ax;
	unsigned long cx;
	unsigned long dx;
	unsigned long si;
	unsigned long di;
	/*
	 * On syscall entry, this is syscall#. On CPU exception, this is error code.
	 * On hw interrupt, it's IRQ number:
	 */
	unsigned long orig_ax;
	/* Return frame for iretq */
	unsigned long ip;
	unsigned long cs;
	unsigned long flags;
	unsigned long sp;
	unsigned long ss;
	/* top of stack page */
};

struct file_mapping_info {
	char filename[MAX_FILE_PATH_NAME];
	unsigned long addr;
	unsigned long len;
	unsigned long prot;
	unsigned long flag;
	unsigned long offset;
};

struct fork_msg_struct {
	u32	pid;
	u32	tgid;
	u32     pid_ns_id;
	u32     prev_pid;
	u32	prev_tgid;
	u32	clone_flags;
	u64     clear_child_tid;
	char	comm[TASK_COMM_LEN];

	//below are for multi-threading

	//hardware context
	struct pt_regs regs;
	unsigned long sp;
	unsigned short ds, es, fsindex, gsindex;
	u32 status;
	unsigned long fsbase, gsbase;
#ifndef BF_CONTROLLER
	struct desc_struct tls_array[GDT_ENTRY_TLS_ENTRIES];
#else
	struct user_desc_struct tls_array[GDT_ENTRY_TLS_ENTRIES];
#endif

	//mm meta
	unsigned long hiwater_rss;      /* High-watermark of RSS usage */
	unsigned long hiwater_vm;       /* High-water virtual memory usage */
	unsigned long total_vm;         /* Total pages mapped */
	unsigned long locked_vm;        /* Pages that have PG_mlocked set */
	unsigned long pinned_vm;        /* Refcount permanently increased */
	unsigned long data_vm;          /* VM_WRITE & ~VM_SHARED & ~VM_STACK */
	unsigned long exec_vm;          /* VM_EXEC & ~VM_WRITE & ~VM_STACK */
	unsigned long stack_vm;         /* VM_STACK */
	unsigned long def_flags;
	unsigned long start_code, end_code, start_data, end_data;
	unsigned long start_brk, brk, start_stack;
	unsigned long arg_start, arg_end, env_start, env_end;
	unsigned long mmap_base, mmap_legacy_base;

	//file mappings & FPU(TODO)
	u32 num_file_mappings;
	struct file_mapping_info file_mapping_infos;
} __packed;

struct fork_reply_struct {
	int			ret;		// error code
	u32			vma_count;	// number of copied vma
	struct mind_map_msg maps[MAX_MAPS_IN_REPLY];
} __packed;

struct remote_thread_reply_struct {
	int         ret;
} __packed;

#ifndef BF_CONTROLLER
struct fork_req_struct {
	struct fork_msg_struct *fork_msg;
	struct list_head node;
};
#endif
#endif
