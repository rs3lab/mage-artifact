/*
 * mm/mmap_disagg.c
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/backing-dev.h>
#include <linux/mm.h>
#include <linux/vmacache.h>
#include <linux/shm.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/syscalls.h>
#include <linux/capability.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/personality.h>
#include <linux/security.h>
#include <linux/hugetlb.h>
#include <linux/shmem_fs.h>
#include <linux/profile.h>
#include <linux/export.h>
#include <linux/mount.h>
#include <linux/mempolicy.h>
#include <linux/rmap.h>
#include <linux/mmu_notifier.h>
#include <linux/mmdebug.h>
#include <linux/perf_event.h>
#include <linux/audit.h>
#include <linux/khugepaged.h>
#include <linux/uprobes.h>
#include <linux/rbtree_augmented.h>
#include <linux/notifier.h>
#include <linux/memory.h>
#include <linux/printk.h>
#include <linux/userfaultfd_k.h>
#include <linux/moduleparam.h>
#include <linux/pkeys.h>
#include <linux/oom.h>

#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/tlb.h>
#include <asm/mmu_context.h>
#include <asm/page_types.h>

#include "internal.h"
#include <disagg/config.h>
#include <disagg/cnmap_disagg.h>
#include <disagg/exec_disagg.h>
#include <disagg/network_disagg.h>
#include <disagg/mmap_disagg.h>
#include <disagg/rmem_disagg.h>
#include <disagg/cnthread_disagg.h>
#include <disagg/print_disagg.h>
#include <disagg/fault_disagg.h>
#include <disagg/profile_points_disagg.h>

#ifndef arch_mmap_check
#define arch_mmap_check(addr, len, flags)	(0)
#endif

void DEBUG_print_vma(struct mm_struct *mm)
{
	int i = 0;
	struct vm_area_struct *ln, *rn, *cur = mm->mmap;

	for(;cur;cur = cur->vm_next)
	{
		ln = cur->vm_rb.rb_left ? rb_entry(cur->vm_rb.rb_left, struct vm_area_struct, vm_rb) : 0;
		rn = cur->vm_rb.rb_right ? rb_entry(cur->vm_rb.rb_right, struct vm_area_struct, vm_rb) : 0;
		pr_info("  *[%d, %p] addr: 0x%lx - 0x%lx [pR/W: %d/%d], l: %p, r: %p\n",
				i, cur, cur->vm_start, cur->vm_end, 
				cur ? ((cur->vm_flags & VM_READ) ? 1 : 0) : -1,
				cur ? ((cur->vm_flags & VM_WRITE) ? 1 : 0) : -1,
				ln, rn);
		i++;
	}
}

static void print_page_checksum(void *data_ptr, unsigned long addr)
#ifdef MIND_VERIFY_PAGE_CHKSUM
{
	unsigned long checksum = 0, *itr;
	for (itr = data_ptr; (char *)itr != ((char *)data_ptr + PAGE_SIZE); ++itr)
		checksum += *itr;
	pr_info("addr[%lx] checksum[%lx]\n", addr, checksum);
}
#else
{}
#endif

// y: This function copies data from a file into the given virtual address
//    range. It's useful for when you mmap in a writeable file; and now need to
//    copy that data into the MIND address space.
static int mmap_copy_page_data_to_mn_from_file(struct task_struct *tsk, struct file *vm_file,
		unsigned long addr, unsigned long len, unsigned long pgoff)
{
	struct fault_data_struct payload;
	struct fault_reply_struct reply;
	int ret = 0;
	size_t data_size = PAGE_SIZE;
	void *data_ptr = NULL;
	long bytes;
	loff_t pos;
	unsigned long tmp_addr = addr;
	int eof = 0;

	data_ptr = kzalloc(data_size, GFP_KERNEL);
	if (!data_ptr) {
		ret = -ENOMEM;
		goto out;
	}

	payload.pid = tsk->pid;
	payload.tgid = tsk->tgid;
	payload.data_size = (u32)data_size;
	payload.data = data_ptr;

	pos = (pgoff << PAGE_SHIFT);
	for (; !eof && (tmp_addr < addr + len); tmp_addr += data_size) { /*do we increament pos here?*/
		memset(data_ptr, 0 , data_size);
		bytes = kernel_read(vm_file, data_ptr, data_size, &pos);
		if (bytes != data_size) {
			ret = bytes;
			pr_err("read %ldB but need %ldB\n", bytes, data_size);
			eof = 1;
			//goto out;
		}
		print_page_checksum(data_ptr, tmp_addr);

		payload.address = get_cnmapped_addr(tmp_addr);
		BUG_ON(payload.address == 0); // CNMapping error!

		// y: This call's local buffer (aka payload.data) is a regular address, not
		//    a DMAable address. Why do you care? Because sometimes
		//    send_msg_to_memory needs a DMA address depending on the RDMA
		//    backend...and you shouldn't worry about that here.
		ret = send_msg_to_memory(DISAGG_DATA_PUSH, &payload, data_size,
				&reply, sizeof(reply));
		if (ret < 0) {
			pr_err("Cannot send page data - err: %d\n", ret);
			goto out;
		}
		pr_info("page[%lx] sent from file pos[%lx] ret[%d]\n", tmp_addr, (unsigned long)pos, ret);
	}

out:
	if (data_ptr)
		kfree(data_ptr);
	return ret;
}


// This function initializes a remote memory region.
// It sets up relevant CN_VA -> MN_VA address translations,
// then initializes the remote memory.
int mmap_disagg__init_rmem(struct task_struct *tsk, unsigned long addr, unsigned long len,
		unsigned long pgoff, struct file *file, bool is_writable_file_map)
{
	int ret = rmem_alloc(tsk->tgid, addr, len);
	if (ret)
		 return ret;

	// Initialize the memory contents.
	if (is_writable_file_map)
		 ret = mmap_copy_page_data_to_mn_from_file(tsk, file, addr, len, pgoff);
	else
		 ret = zero_rmem_region(tsk, addr, len);

	return ret;
}

/*
 * Disaggregated munmap
 */
int mmap_disagg__free_rmem(struct task_struct *tsk, unsigned long start, size_t len)
{
	// TODO(yash): This function should only destroy _part_ of a mapping, if it eliminates only part of
	// that mapping...
	return rmem_free(tsk->tgid, start, len);
}

/*
 * Disaggregated mremap
 */
static unsigned long send_mremap_to_mn(struct task_struct *tsk, 
			unsigned long addr, unsigned long old_len,
			unsigned long new_len, unsigned long flags,
			unsigned long new_addr)
{
	struct mremap_msg_struct payload;
	struct mremap_reply_struct *reply;
	unsigned long ret_addr = 0;
	int ret = -1;

	reply = kzalloc(sizeof(struct mremap_reply_struct), GFP_KERNEL);
	if (!reply)
		 return -ENOMEM;

	payload.pid = tsk->pid;
	payload.tgid = tsk->tgid;
	payload.pid_ns_id = tsk->pid_ns_id;
	payload.addr = addr;
	payload.old_len = old_len;
	payload.new_len = new_len;
	payload.flags = flags;
	payload.new_addr = new_addr;

	// y: I need to implement the memory movement on the CN side before
	// this will work.
	pr_warn("WARNING: unsupported disagg mremap called!\n");

	// Check the size of the received data
	ret = send_msg_to_control(DISAGG_MREMAP, &payload, sizeof(payload),
			reply, sizeof(*reply));
	pr_syscall(KERN_DEFAULT "MREMAP - Data from CTRL [%d]: ret: %d, addr: 0x%lx [0x%llx]\n",
			ret, reply->ret, reply->new_addr, *(long long unsigned *)(reply));

	if (ret >=0 && !reply->ret) {  // Only 0 is success
		 ret_addr = reply->new_addr;

		 pr_maps("CNMAPS: Updating maps after `mremap()` syscall\n");
		 set_cnmaps(reply->maps, MAX_MAPS_IN_REPLY);
		 print_cnmaps();
	}

	kfree(reply);
	return ret_addr;
}

unsigned long disagg_mremap(struct task_struct *tsk, 
			unsigned long addr, unsigned long old_len,
			unsigned long new_len, unsigned long flags,
			unsigned long new_addr)
{
	return send_mremap_to_mn(tsk, addr, old_len, new_len, flags, new_addr);
}

__always_inline int TEST_is_target_vma(unsigned long vm_start, unsigned long vm_end)
{
	return ((vm_end - vm_start) >= TEST_INIT_ALLOC_SIZE) ? 1 : 0;
}

__always_inline int TEST_is_sub_target_vma(unsigned long vm_start, unsigned long vm_end)
{
	return ((vm_end - vm_start) == TEST_SUB_REGION_ALLOC_SIZE) ? 1 : 0;
}

__always_inline int TEST_is_meta_vma(unsigned long vm_start, unsigned long vm_end)
{
	return ((vm_end - vm_start) == TEST_META_ALLOC_SIZE) ? 1 : 0;
}

__always_inline int TEST_is_test_vma(unsigned long vm_start, unsigned long vm_end)
{
	return (TEST_is_target_vma(vm_start, vm_end) || TEST_is_sub_target_vma(vm_start, vm_end) 
			|| TEST_is_meta_vma(vm_start, vm_end));
}
