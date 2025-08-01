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

#include <linux/mm.h>
#include <linux/rmap.h>
#include <linux/mempolicy.h>
#include <asm/pgtable_types.h>
#include <asm/pgtable.h>
#include <asm/mman.h>

int count_vm_field(struct task_struct *tsk)
{
	int tot_num = 0;
	struct vm_area_struct *mpnt;
	for (mpnt = tsk->mm->mmap; mpnt; mpnt = mpnt->vm_next)
	{
		tot_num ++;
	}
	return tot_num;
}

int init_vma_field(struct exec_vmainfo *_vma_buf, struct task_struct *tsk)
{
	int res = 0;
	struct vm_area_struct *mpnt;
	struct exec_vmainfo *vma_buf = _vma_buf;
	for (mpnt = tsk->mm->mmap; mpnt; vma_buf++, mpnt = mpnt->vm_next)
	{
		if (mpnt->vm_start <= tsk->mm->start_stack && tsk->mm->start_stack < mpnt->vm_end)
		{
			// expand stack
			unsigned long stack_size = 8 * 1024 * 1024;	// pre-allocatedc 8 MB
			if (expand_stack(mpnt, mpnt->vm_end - stack_size))
			{
				BUG();
			}
		}
		// TODO: copy other important information
		vma_buf->vm_start = mpnt->vm_start;
		vma_buf->vm_end = mpnt->vm_end;
		vma_buf->vm_flags = mpnt->vm_flags;
		vma_buf->vm_pgoff = mpnt->vm_pgoff;
		vma_buf->rb_substree_gap = mpnt->rb_subtree_gap;
		vma_buf->vm_page_prot = mpnt->vm_page_prot.pgprot;
		// use file pointer as an identifier
		vma_buf->file_id = (unsigned long)(mpnt->vm_file);

		//for multithreading, make switch recard writable file mappings also as anonymous mapping,
		//so that we won't have a permission fault when remotely accessing writable file mappings.
		//another way is to change the permission check rules on the switch
		if (mpnt->vm_flags & VM_WRITE) {

			vma_buf->file_id = 0;
		}

		// printk(KERN_DEFAULT "vma copy to: 0x%lx", (long unsigned int)vma_buf);
		//print out
		// printk("vma[%ld] [%lx, %lx] perm[%lx] file[%lx]\n", (long)(vma_buf - _vma_buf),
		// 		vma_buf->vm_start, vma_buf->vm_end, vma_buf->vm_flags, vma_buf->file_id);
	}

	return res;
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

static void cn_set_up_layout(struct exec_msg_struct* payload,
							 struct mm_struct *mm)
{
	CN_COPY_MM_VALUES(payload, mm, hiwater_rss);
	CN_COPY_MM_VALUES(payload, mm, hiwater_vm);
	CN_COPY_MM_VALUES(payload, mm, total_vm);
	CN_COPY_MM_VALUES(payload, mm, locked_vm);
	CN_COPY_MM_VALUES(payload, mm, pinned_vm);
	CN_COPY_MM_VALUES(payload, mm, data_vm);
	CN_COPY_MM_VALUES(payload, mm, exec_vm);
	CN_COPY_MM_VALUES(payload, mm, stack_vm);
	CN_COPY_MM_VALUES(payload, mm, def_flags);
	CN_COPY_MM_VALUES(payload, mm, start_code);
	CN_COPY_MM_VALUES(payload, mm, end_code);
	CN_COPY_MM_VALUES(payload, mm, start_data);
	CN_COPY_MM_VALUES(payload, mm, end_data);
	CN_COPY_MM_VALUES(payload, mm, start_brk);
	CN_COPY_MM_VALUES(payload, mm, brk);
	CN_COPY_MM_VALUES(payload, mm, start_stack);
	CN_COPY_MM_VALUES(payload, mm, arg_start);
	CN_COPY_MM_VALUES(payload, mm, arg_end);
	CN_COPY_MM_VALUES(payload, mm, env_start);
	CN_COPY_MM_VALUES(payload, mm, env_end);
	CN_COPY_MM_VALUES(payload, mm, mmap_base);
	CN_COPY_MM_VALUES(payload, mm, mmap_legacy_base);
}

// DEFINE_PP(exec_send_to_memory);

static DEFINE_MUTEX(exec_reply_mutex);
// y: This struct is huge. So we allocate it once statically instead of on the stack.
//    TODO just kmalloc this instead once we've fixed the mmap_sem sleep while atomic issue.
static struct exec_reply_struct exec_reply;

int copy_vma_to_mn(struct task_struct *tsk, u32 msg_type)
{
	struct exec_msg_struct *payload;
	struct exec_reply_struct *reply = &exec_reply;
	int ret = 0;	//rdma_ret
	size_t tot_size = sizeof(struct exec_msg_struct);

	mutex_lock(&exec_reply_mutex);
	memset(reply, 0, sizeof(*reply));

	// PP_STATE(exec_send_to_memory);

	if (msg_type != DISAGG_CHECK_VMA)
	{
		ret = count_vm_field(tsk);
		if (ret > 0)
			 tot_size += sizeof(struct exec_vmainfo) * (ret-1);
	}
	// printk(KERN_DEFAULT "EXEC - VMA SIZE: %lu", tot_size);

	// calculate number of vmas
	// allocate size: struct size + vmas
	payload = (struct exec_msg_struct*)kmalloc(tot_size, GFP_KERNEL);
	if (!payload) {
		mutex_unlock(&exec_reply_mutex);
		return -ENOMEM;
	}

	payload->pid = tsk->pid;
	payload->tgid = tsk->tgid;
	payload->pid_ns_id = tsk->pid_ns_id;
	memcpy(payload->comm, tsk->comm, TASK_COMM_LEN);
	cn_set_up_layout(payload, tsk->mm);

	// put vma information
	payload->num_vma = (u32)ret;
	if (msg_type != DISAGG_CHECK_VMA)
		 init_vma_field(&payload->vmainfos, tsk);

	ret = send_msg_to_control(msg_type, payload, tot_size,
			reply, sizeof(*reply));

	// Now, ret is the received length (may not for RDMA)
	if (ret < 0)
	{
		printk(KERN_ERR "Cannot send EXEC notification - err: %d [%s]\n", 
				ret, tsk->comm);
		// printk(KERN_ERR "** EXEC - Data from RDMA [%d]: [0x%llx]\n",
		// 		rdma_ret, *(long long unsigned*)(reply));
		goto cn_notify_out;
	}
	ret = 0;

	pr_maps("CNMAPS: Updating maps after `{fork,exec}()` syscall.\n");
	set_cnmaps(reply->maps, MAX_MAPS_IN_REPLY);
	print_cnmaps();

cn_notify_out:
	mutex_unlock(&exec_reply_mutex);
	kfree(payload);
	return ret;
}

static int exec_copy_page_data_to_mn(u16 tgid, struct mm_struct *mm, unsigned long addr,
									 pte_t *pte)
{
	pr_syscall("exec_copy_page_data_to_mn(tgid, mm, addr=0x%lx, pte)\n", addr);
	return cn_copy_page_data_to_mn(tgid, mm, addr, pte, CN_ONLY_DATA, 0, NULL);
}

/*
static void print_page_checksum(void *data_ptr, unsigned long addr, unsigned long dma_addr, int target)
#ifdef MIND_VERIFY_PAGE_CHKSUM
{
	unsigned long checksum = 0, *itr;
	for (itr = data_ptr; (char *)itr != ((char *)data_ptr + PAGE_SIZE); ++itr)
		checksum += *itr;
	pr_info("invchecksum addr[%lx] checksum[%lx] dma_addr[%lx] target[%d]\n", addr, checksum, dma_addr, target);
}
#else
{}
#endif
*/

/* It copy data from file for a particular vma */
static int cn_copy_page_data_to_mn_from_file(u16 tgid,
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
int cn_copy_vma_data_to_mn(struct task_struct *tsk, struct vm_area_struct *vma, 
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
		return cn_copy_page_data_to_mn_from_file(tsk->tgid, vma, start_addr, off_in_vma);
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
 *
 * @is_exec: reset VMAs and clean up all the cachelines for this tgid
 */
// 
static int _cn_notify_fork_exec(struct task_struct *tsk, int is_exec)
{
	struct vm_area_struct *cur = tsk->mm->mmap;
	struct vm_area_struct *prev, *next;
	struct mm_struct *mm = tsk->mm;

	int ret = 0;
	if (is_exec)
		ret = copy_vma_to_mn(tsk, DISAGG_EXEC);

	// no error, now all mapping are stored in memory node
	BUG_ON(unlikely(ret));

	// disagg_print_va_layout(tsk->mm);

	// For all of the existing task's VMAs
	while (cur)
	{
		//pr_info("cur[%lx, %lx]\n", cur->vm_start, cur->vm_end);
		next = cur->vm_next;
		prev = cur->vm_prev;
		// printk(KERN_DEFAULT "VMA: tgid[%5u] VA[0x%lx - 0x%lx] Flag[0x%lx] File[%d]\n",
		// 	   tsk->tgid, cur->vm_start, cur->vm_end, cur->vm_flags, cur->vm_file ? 1 : 0);

		// y: remove existing anon & writeable vmas.
		if ((vma_is_anonymous(cur) && 
					((cur->vm_flags & VM_WRITE) || // writable page
					 !(cur->vm_flags & (VM_WRITE | VM_READ | VM_EXEC)))) // pages in software DRAM page = no permission
				|| (tsk->is_remote && (cur->vm_flags & VM_WRITE)))
		{
			int sent = -1;
			int stack = 0;
			unsigned long address, res_addr;

			// Phase 1: Push the VMA data to the MN

			if (cur->vm_end >= cur->vm_start) {
				if (cur->vm_end - cur->vm_start > DISAGG_NET_MAX_SIZE_ONCE) {
					unsigned long offset = 0;

					while (cur->vm_start + offset < cur->vm_end) {
						sent = cn_copy_vma_data_to_mn(tsk, cur,
								cur->vm_start + offset,
								min(cur->vm_start + offset + DISAGG_NET_MAX_SIZE_ONCE, cur->vm_end),
								offset);
						if(sent)
							 break;
						offset += DISAGG_NET_MAX_SIZE_ONCE;
					}
				} else {
					sent = cn_copy_vma_data_to_mn(tsk, cur, cur->vm_start, cur->vm_end, 0);
				}
			}
			if (sent) { // 0: successfully sent, -EINTR: no pte populated
				pr_syscall("**WARN: cannot send vma data: 0x%lx - 0x%lx [%lu]\n",
						cur->vm_start, cur->vm_end, cur->vm_end - cur->vm_start);
				goto next_vma;
			}

			pr_syscall("done sending vma data to mn [%lx, %lx] sent[%d]", cur->vm_start, cur->vm_end, sent);

			// Phase 2: remove previous mappings.
			// y: We disabled this in a "HACK" commit.

			if((cur->vm_flags & (VM_SHARED | VM_PFNMAP))) {
				//special flags
				// pr_syscall("Do-not remove special writable vma: 0x%lx - 0x%lx [file:%d][flag:0x%lx]\n",
				// 		cur->vm_start, cur->vm_end, cur->vm_file ? 1 : 0, cur->vm_flags);
				goto next_vma;
			}

			address = cur->vm_start;
			res_addr = address;
			if (cur->vm_start <= mm->start_stack && mm->start_stack < cur->vm_end)
				 stack = 1;

			//pr_info("start unmap vma [%lx, %lx] stack[%d]", cur->vm_start, cur->vm_end, stack);
			// printk(KERN_DEFAULT "Remove pages in writable vma: 0x%lx - 0x%lx [flag: 0x%lx, pgoff: 0x%lx, stack: %d]\n",
			// 	   cur->vm_start, cur->vm_end, cur->vm_flags, cur->vm_pgoff, stack);

			cur = find_vma(mm, address);
			// printk(KERN_DEFAULT "find_vma: cur is updated to 0x%lx\n", (unsigned long)cur);
			if (unlikely(!cur || (cur && cur->vm_start > address))) {
				printk(KERN_ERR "Failed to initialize clean mmap [0x%lx]: 0x%lx, addr: 0x%lx, res_addr: 0x%lx\n",
						address, (unsigned long)cur, cur ? cur->vm_start : 0, res_addr);
				if (cur && cur->vm_prev)
					 printk(KERN_ERR "prev vma: 0x%lx - 0x%lx\n",
							 cur->vm_prev->vm_start, cur->vm_prev->vm_end);
				BUG();
			}

			if(tsk->is_remote)
				 // pr_info("dummy vma[%lx, %lx] file[%d] flags[%lx] pgprot[%lx]\n",
					// 	 cur->vm_start, cur->vm_end, cur->vm_file ? 1:0,
					// 	 cur->vm_flags, (unsigned long)(cur->vm_page_prot.pgprot));

			// pr_info("done creating dummy vma [%lx, %lx]", cur->vm_start, cur->vm_end);
			goto next_vma;
		}

		if (cur->vm_flags & VM_WRITE){
			if (!cur->vm_file){	// print out errorous case only
				pr_syscall("**WARN: non-anon & non-file but writable (f:%d): 0x%lx - 0x%lx\n",
						cur->vm_file ? 1 : 0, cur->vm_start, cur->vm_end);
			}
			goto next_vma;
		}

		if (vma_is_anonymous(cur)){
			pr_syscall("**WARN: anon but read-only: 0x%lx - 0x%lx\n",
					cur->vm_start, cur->vm_end);
			// NOTE: COW has read only PTE but writable VMA
			goto next_vma;
		}
next_vma:
		cur = next;
		// printk(KERN_DEFAULT "cur = next: cur is updated to 0x%lx\n", (unsigned long)cur);
	}

	if (is_exec)
		cnthread_clean_up_non_existing_entry(tsk->tgid, mm);

	//pr_info("start print tsk->mm[%p]\n", tsk->mm);
	disagg_print_va_layout(tsk->mm);

	// HACK: Used for quick pre-deadline bug fix (need quick way to find mm of disagg process).
	//       Remove this as soon as you can.
	disagg_mm = tsk->mm;

	//pr_info("done print\n");
	return ret;
}

int cn_notify_exec(struct task_struct *tsk)
{
	return _cn_notify_fork_exec(tsk, 1);
}

int cn_notify_fork(struct task_struct *tsk)
{
	return _cn_notify_fork_exec(tsk, 0);
}
/* vim: set tw=99 */
