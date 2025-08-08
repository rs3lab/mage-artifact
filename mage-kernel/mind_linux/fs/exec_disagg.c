/*
 * Header file of exec and disaggregated exec functions
 */
#include <linux/exec.h>
#include <disagg/config.h>
#include <disagg/cnmap_disagg.h>
#include <disagg/exec_disagg.h>
#include <disagg/network_disagg.h>
#include <disagg/fault_disagg.h>
#include <disagg/cnthread_disagg.h>
#include <disagg/profile_points_disagg.h>
#include <disagg/print_disagg.h>
#include <disagg/rmem_disagg.h>

#include <linux/mm.h>
#include <linux/rmap.h>
#include <linux/mempolicy.h>
#include <asm/pgtable_types.h>
#include <asm/pgtable.h>
#include <asm/mman.h>

// This function takes a `task`, and expands its stack VMA to 8 MiB (just like MIND).
// Data path is not touched, memory is not initialized; only the struct VMA is expanded.
static void expand_stack_vma(struct task_struct *tsk)
{
	struct vm_area_struct *vma;
	for (vma = tsk->mm->mmap; vma; vma = vma->vm_next)
	{
		if (vma->vm_start <= tsk->mm->start_stack && tsk->mm->start_stack < vma->vm_end) {
			unsigned long stack_size = 8 * 1024 * 1024;	
			if (expand_stack(vma, vma->vm_end - stack_size))
				BUG();
		}
	}
}

void disagg_print_va_layout(struct mm_struct *mm)
{
	/*
	pr_syscall("** CN-VA layout **\n");
	pr_syscall("-total: %lu pages\n", mm->total_vm);
	pr_syscall("-code: 0x%lx - 0x%lx\n", mm->start_code, mm->end_code);
	pr_syscall("-data: 0x%lx - 0x%lx\n", mm->start_data, mm->end_data);
	pr_syscall("-brk: 0x%lx - 0x%lx\n", mm->start_brk, mm->brk);
	pr_syscall("-stack: 0x%lx\n", mm->start_stack);
	pr_syscall("-arg: 0x%lx - 0x%lx\n", mm->arg_start, mm->arg_end);
	pr_syscall("-env: 0x%lx - 0x%lx\n", mm->env_start, mm->env_end);
	*/
	pr_info("** CN-VA layout **\n");
	pr_info("-total: %lu pages\n", mm->total_vm);
	pr_info("-code: 0x%lx - 0x%lx\n", mm->start_code, mm->end_code);
	pr_info("-data: 0x%lx - 0x%lx\n", mm->start_data, mm->end_data);
	pr_info("-brk: 0x%lx - 0x%lx\n", mm->start_brk, mm->brk);
	pr_info("-stack: 0x%lx\n", mm->start_stack);
	pr_info("-arg: 0x%lx - 0x%lx\n", mm->arg_start, mm->arg_end);
	pr_info("-env: 0x%lx - 0x%lx\n", mm->env_start, mm->env_end);
}

#define CN_COPY_MM_VALUES(EXR, MM, F)	(EXR->F = MM->F)


static int exec_copy_page_data_to_mn(u16 tgid, struct mm_struct *mm, unsigned long addr,
									 pte_t *pte)
{
	pr_syscall("exec_copy_page_data_to_mn(tgid, mm, addr=0x%lx, pte)\n", addr);
	return cn_copy_page_data_to_mn(tgid, mm, addr, pte, CN_ONLY_DATA, 0, NULL);
}


/* It copy data from file for a particular vma */
static int copy_vma_page_to_rmem_from_file(u16 tgid,
        struct vm_area_struct *vma, unsigned long addr, off_t off_in_vma)
{
	struct fault_data_struct request;
	struct fault_reply_struct reply;
	int ret = 0;
	size_t data_size = PAGE_SIZE;
	void *data_ptr = NULL;
	long bytes;
	loff_t pos;

	// pr_syscall("cn_copy_vma_data_to_mn_from_file(tsk, vma, addr=0x%lx, off=%ld)\n",
	// 		addr, off_in_vma);
	//
	// y: Note: The network stack requires this allocation to be page-aligned.
	//    In practice this seems to happen; but if that every changes, fix this allocation.
	data_ptr = kzalloc(data_size, GFP_KERNEL);
	if (!data_ptr) {
		ret = -ENOMEM;
		goto out;
	}

	// pr_syscall("cn_copy_page_data_to_mn_from_file: payload.data=0x%lx\n", (unsigned long) data_ptr);

	request.pid = tgid;
	request.tgid = tgid;
	request.address = get_cnmapped_addr(addr);
	BUG_ON(!request.address); // CNMapping error!
	request.data_size = (u32)data_size;
	request.data = data_ptr;

	pos = (vma->vm_pgoff << PAGE_SHIFT) + off_in_vma;

	bytes = kernel_read(vma->vm_file, data_ptr, data_size, &pos);

	if (bytes != data_size) {
		ret = bytes;
		pr_err("fail to read writable file mapping from file\n");
		goto out;
	}


	// y: This call's local buffer (aka payload.data) is a regular address, not
	//    a DMAable address. Why do you care? Because sometimes
	//    send_msg_to_memory needs a DMA address depending on the RDMA
	//    backend...and you shouldn't worry about that here.
	ret = send_msg_to_memory(DISAGG_DATA_PUSH, &request, data_size,
			&reply, sizeof(reply));
	if (ret < 0) {
		pr_err("Cannot send page data - err: %d\n", ret);
		goto out;
	}
out:
	if (data_ptr)
		 kfree(data_ptr);
	// pr_info("page sent from file %lx ret[%d]\n", addr, ret);
	return ret;
}

/* It copy data from its own memory, only one page though. */
int copy_vma_page_to_rmem(struct task_struct *tsk, struct vm_area_struct *vma, 
		unsigned long start_addr, unsigned long end_addr, off_t off_in_vma)
{
	pte_t *pte = NULL;

	pr_rdma("cn_copy_vma_data_to_mn(tsk, vma, start=0x%lx, end=0x%lx, off=%ld)\n",
			start_addr, end_addr, off_in_vma);

	if (end_addr <= start_addr)
		return 0;	//no data to send

	// find pte
	pte = find_pte_target(tsk->mm, start_addr);
	if (pte && !pte_none(*pte)) //check for tsk
	{
		// forked, so same address but from the previous tsk (current)
		pte = find_pte_target(current->mm, start_addr);
		if (pte && !pte_none(*pte) && pte_present(*pte)) //check for cur
		{
			pr_rdma("Copying page data to mn (not from file)\n");
			// TODO: we will need to grab pte lock before go into cn_copy_page_data_to_mn()
			//		 and unlock it after return from cn_copy_page_data_to_mn()
			return exec_copy_page_data_to_mn(tsk->tgid, tsk->mm, start_addr, pte);
		}
	}

	//if we reach here, then the page can not be sent from memory,
	//which means the mapping of page frame has not been established,
	//which means for file mappings, the file content == the initial content in memory
	//so instead, we can send it from file
	if (vma->vm_file) {
		pr_rdma("Copying page data to mn (from file)\n");
		return copy_vma_page_to_rmem_from_file(tsk->tgid, vma, start_addr, off_in_vma);
	}
	return 0;	// no pte to send
}

/*
	if (pte && !pte_none(*pte))
	{
	page = pte_page(*pte);
	}
	page = phys_to_page(dma_addr);
	if (page)
	{
	void *tmp_data = kmap_atomic(page);
	print_page_checksum(tmp_data, addr, (u64)dma_addr, is_target_data);
	kunmap_atomic(tmp_data);
	}
*/

/* It copy data from (maybe) other process's memory */
int cn_copy_page_data_to_mn(u16 tgid, struct mm_struct *mm,
		unsigned long remote_addr, pte_t *pte, int is_target_data,
		u32 req_qp, void *local_addr)
{
	struct fault_data_struct payload;
	struct fault_reply_struct reply;   //dummy buffer for ack
	int ret;
	u32 msg_type;
	struct page *page = NULL;
	unsigned long *tmp_kmap = NULL;

	payload.data = local_addr;
	payload.pid = tgid;	//fake
	payload.tgid = tgid;
	payload.address = get_cnmapped_addr(remote_addr);
	BUG_ON(!payload.address); // cnmapping error!
	payload.data_size = (u32) PAGE_SIZE;

	switch (is_target_data) {
	case CN_ONLY_DATA:
		msg_type = DISAGG_DATA_PUSH;
		break;
	case CN_OTHER_PAGE:
		msg_type = DISAGG_DATA_PUSH_OTHER;
		break;
	default:
		BUG();
		break;
	}

	if (!payload.data) {
		pr_rdma("Assigning new kmapped page for rdma buffer...\n");
		if (pte && !pte_none(*pte))
			 page = pte_page(*pte);
		if (page) {
			tmp_kmap = (unsigned long *)kmap(page);
			payload.data = (void *)tmp_kmap;
		}
	}

	if (!payload.data) {
		ret = -EINTR;
		goto out;
	}
	pr_rdma("cn_copy_page_data_to_mn(tgid, remote_addr=0x%lx, local_addr=0x%lx\n", remote_addr,
			(unsigned long) payload.data);
	// Now, ret is the received length
	ret = send_msg_to_memory(msg_type, &payload, PAGE_SIZE, &reply, sizeof(reply));
	if (ret < 0) {
		pr_warn_ratelimited("Cannot send page data - err: %d, type: %d, dma: 0x%lx\n",
				ret, is_target_data, (unsigned long)local_addr);
		goto out;
	}
	ret = 0;

out:
	if (tmp_kmap)
		 kunmap(page);
	return ret;
}

struct mm_struct *disagg_mm = NULL;

/*
 * We assume that caller already holds write lock for mm->mmap_sem
 */
void send_vma_data_to_rmem(struct task_struct *tsk)
{
	struct vm_area_struct *cur = tsk->mm->mmap;
	struct vm_area_struct *prev, *next;

	// For all of the existing task's VMAs
	while (cur)
	{
		next = cur->vm_next;
		prev = cur->vm_prev;

		// y: remove existing anon & writeable vmas.
		if ((vma_is_anonymous(cur) && 
					((cur->vm_flags & VM_WRITE) || // writable page
					 !(cur->vm_flags & (VM_WRITE | VM_READ | VM_EXEC)))) // pages in software DRAM page = no permission
				|| (tsk->is_remote && (cur->vm_flags & VM_WRITE)))
		{
			int sent = -1;

			// Phase 1: Push the VMA data to the MN
			if (cur->vm_end >= cur->vm_start) {
				if (cur->vm_end - cur->vm_start > DISAGG_NET_MAX_SIZE_ONCE) {
					unsigned long offset = 0;

					while (cur->vm_start + offset < cur->vm_end) {
						sent = copy_vma_page_to_rmem(tsk, cur,
								cur->vm_start + offset,
								min(cur->vm_start + offset + DISAGG_NET_MAX_SIZE_ONCE,
									cur->vm_end),
								offset);
						if(sent)
							 break;
						offset += DISAGG_NET_MAX_SIZE_ONCE;
					}
				} else {
					sent = copy_vma_page_to_rmem(tsk, cur, cur->vm_start, cur->vm_end, 0);
				}
			}
			BUG_ON(sent);

			// Phase 2: remove previous mappings.
			// y: We disabled this in a "HACK" commit.

			goto next_vma;
		}

		if ((cur->vm_flags & VM_WRITE) && !cur->vm_file) {
			pr_warn("warn: non anon & non-file VMA...but writable?: 0x%lx-0x%lx\n",
					cur->vm_start, cur->vm_end);
			dump_stack();
			goto next_vma;
		}

		// NOTE: COW has read only PTE but writable VMA
		WARN(vma_is_anonymous(cur), "anon but read-only vma: 0x%lx - 0x%lx\n", cur->vm_start, cur->vm_end);
next_vma:
		cur = next;
	}

	disagg_print_va_layout(tsk->mm);
}

static int send_sigkill(struct task_struct *tsk)
{
	struct siginfo info;
	int ret = 0;
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIGKILL;
	ret = send_sig_info(SIGKILL, &info, current);
	if (ret < 0) {
		printk(KERN_INFO "Cannot send kill signal\n");
	}
	return ret;
}

// Allocate remote memory to 'back' every VMA in the process
// TODO: implement more conservative allocations. Eg: MIND only moves _anonymous_ pages to memory,
//       IIRC (iglo). So those shouldn't need backing remote memory!
static void alloc_disagg_rmem(struct task_struct *tsk)
{
	struct vm_area_struct *vma;
	for (vma = tsk->mm->mmap; vma; vma = vma->vm_next) {
		size_t len = vma->vm_end - vma->vm_start;
		BUG_ON(rmem_alloc(tsk->tgid, vma->vm_start, len));
		BUG_ON(zero_rmem_region(tsk, vma->vm_start, len));
	}
}

// Initiates disagg-related functionalities after `exec` syscall.
//
// Assumes `tsk` is remote.
int disagg_exec(struct task_struct *tsk)
{
	struct mm_struct *mm = tsk->mm;
	down_write(&mm->mmap_sem);

	pr_syscall("execve: %s (uid:%d, pid:%d)\n", tsk->comm,
			(int)tsk->cred->uid.val, (int)tsk->pid);

	// Vestigial TLB ASID code...don't mess with this.
	mm->is_remote_mm = true;
	mm->remote_asid = find_next_avail_disagg_asid(mm);
	pr_syscall("MIND - EXEC | New tgid[%u] for ASID[%u]\n", tsk->tgid, mm->remote_asid);

	// HACK: Used for quick pre-deadline bug fix (need quick way to find mm of disagg process).
	//       Remove this as soon as you can.
	disagg_mm = tsk->mm;

	if (mm->start_brk - mm->brk) {
		pr_warn("EXEC: process has already written to heap, committing suicide now.\n");
		send_sigkill(tsk);
		goto out;
	} 

	expand_stack_vma(tsk);
	alloc_disagg_rmem(tsk);
	send_vma_data_to_rmem(tsk);
	free_orphaned_cnpages(tsk->tgid, mm);

out:
	up_write(&mm->mmap_sem);
	return 0;
}

/* vim: set tw=99 */
