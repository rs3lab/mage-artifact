// PARTIALLY DUPLICATED FROM original fault.c //
#include <linux/sched.h>		/* test_thread_flag(), ...	*/
#include <linux/sched/task_stack.h>	/* task_stack_*(), ...		*/
#include <linux/kdebug.h>		/* oops_begin/end, ...		*/
#include <linux/extable.h>		/* search_exception_tables	*/
#include <linux/bootmem.h>		/* max_low_pfn			*/
#include <linux/kprobes.h>		/* NOKPROBE_SYMBOL, ...		*/
#include <linux/mmiotrace.h>		/* kmmio_handler, ...		*/
#include <linux/perf_event.h>		/* perf_sw_event		*/
#include <linux/hugetlb.h>		/* hstate_index_to_shift	*/
#include <linux/prefetch.h>		/* prefetchw			*/
#include <linux/context_tracking.h>	/* exception_enter(), ...	*/
#include <linux/uaccess.h>		/* faulthandler_disabled()	*/
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/rmap.h>
#include <linux/swap.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/mmu_notifier.h>	/* ptep_clear_flush_notify ... */
#include <linux/random.h>

#include <linux/smp.h>

#include <asm/cpufeature.h>		/* boot_cpu_has, ...		*/
#include <asm/traps.h>			/* dotraplinkage, ...		*/
#include <asm/pgalloc.h>		/* pgd_*(), ...			*/
#include <asm/fixmap.h>			/* VSYSCALL_ADDR		*/
#include <asm/vsyscall.h>		/* emulate_vsyscall		*/
#include <asm/vm86.h>			/* struct vm86			*/
#include <asm/mmu_context.h>		/* vma_pkey()			*/
#include <asm-generic/memory_model.h>

// #define CREATE_TRACE_POINTS
// #include <asm/trace/exceptions.h>

#include "mm_internal.h"
#include <disagg/config.h>
#include <disagg/cnmap_disagg.h>
#include <disagg/network_disagg.h>
#include <disagg/network_fit_disagg.h>
#include <disagg/fault_disagg.h>
#include <disagg/exec_disagg.h>
#include <disagg/cnthread_disagg.h>
#include <disagg/kshmem_disagg.h>
#include <disagg/print_disagg.h>
#include <disagg/profile_points_disagg.h>
#include <disagg/range_lock_disagg.h>

extern noinline void    // was static
bad_area_nosemaphore(struct pt_regs *regs, unsigned long error_code,
		     unsigned long address, u32 *pkey);

extern noinline void    // was static
bad_area(struct pt_regs *regs, unsigned long error_code, unsigned long address);

extern noinline void    // was static
bad_area_access_error(struct pt_regs *regs, unsigned long error_code,
		      unsigned long address, struct vm_area_struct *vma);

extern inline int    // was static
access_error(unsigned long error_code, struct vm_area_struct *vma);

extern noinline void    // was static
no_context(struct pt_regs *regs, unsigned long error_code,
	   unsigned long address, int signal, int si_code);

extern noinline void    // was static
mm_fault_error(struct pt_regs *regs, unsigned long error_code,
	       unsigned long address, u32 *pkey, unsigned int fault);

extern void DEBUG_print_vma(struct mm_struct *mm);

DEFINE_PP(FH_total);
// DEFINE_PP(FH_init_vma);
// DEFINE_PP(FH_rangelock);
// DEFINE_PP(FH_get_pte);
// DEFINE_PP(FH_rdma);
// DEFINE_PP(FH_restore_data_page);
// DEFINE_PP(FH_cleanup);
// DEFINE_PP(FH_wait_for_free_page);
// DEFINE_PP(FH_linux_fh_);
// DEFINE_PP(FH_give_up_count);

// pf := "page fault"
// DEFINE_PP(FH_pf_not_present_count);
// DEFINE_PP(FH_pf_present_count);

// Not just for disagg faults. Counts _all_ faults.

// DEFINE_PP(FH_pdp);

#ifdef MIND_VERIFY_PAGE_CHKSUM
static void print_page_checksum(void *data_ptr, unsigned long addr, unsigned long pfn, unsigned long ret_dma_addr, unsigned long dma_addr, struct cnthread_page *cnpage)
{
	unsigned long checksum = 0, *itr;
	for (itr = data_ptr; (char *)itr != ((char *)data_ptr + PAGE_SIZE); ++itr)
		checksum += *itr;
	pr_pgfault("pfault addr[%lx] checksum[%lx] pfn[%lx] retdmaaddr[%lx] dmaaddr[%lx] cnreq[%llx]\n", addr, checksum, pfn, ret_dma_addr, dma_addr, (u64)cnpage);
}
#endif

static int send_page_fault_to_memory(struct task_struct *tsk, u64 raddr, u64 laddr_dma)
{
	u64 raddr_translated = get_cnmapped_addr(raddr);
	struct mind_rdma_req req;

	BUG_ON(raddr_translated == 0); // cnmapping error!
	BUG_ON(mind_rdma_read_fn(tsk->qp_handle, (void *) laddr_dma, raddr_translated,
				PAGE_SIZE, &req));

	// Handle LATR states until the RDMA ACK comes.
	while (true) {
		int ret = mind_rdma_check_cq_fn(tsk->qp_handle, &req);

		if (ret == 0) {
			patr_poll_fh_queue();
			ndelay(MIND_FH_POLL_BACKOFF_TIME_NS);
			continue;
		}
		if (ret == 1)
			 break;

		pr_err("Error when polling for CQE! raddr: 0x%llx -> 0x%llx\n", raddr, raddr_translated);
		BUG(); 
	}

	return 0;
}

static pte_t *find_pte_from_reg(unsigned long address)
{
	pgd_t *pgd;
	unsigned int level;

	pgd = __va(read_cr3_pa());
	pgd += pgd_index(address);
	return lookup_address_in_pgd(pgd, address, &level);
}

static pte_t *find_pte_from_mm(struct mm_struct *mm, unsigned long address)
{
	pgd_t *pgd;
	unsigned int level;
	if (mm)
	{
		pgd = pgd_offset(mm, address);
		return lookup_address_in_pgd(pgd, address, &level);
	}else{
		return NULL;
	}
}

void print_pgfault_error(struct task_struct *tsk, unsigned long error_code, 
	unsigned long address, struct vm_area_struct *vma, int is_prefetch)
{
	// pte_t *pte = find_pte_from_reg(address);
	pte_t *pte = NULL;
	if (!is_prefetch)
		pte = find_pte_from_mm(is_kshmem_address(address) ? &init_mm : tsk->mm, address);

	printk(KERN_DEFAULT "CN: fault handler Error! - (tgid: %d, pid: %d, W: %d, pR/W: %d/%d, an: %d, pte_fl: 0x%03lx, pfn: 0x%06lx, err: 0x%02lx) - addr: 0x%lx, vma: 0x%lx - 0x%lx [0x%lx], st[0x%lx]\n",
		(int)tsk->tgid, (int)tsk->pid,
		(error_code & X86_PF_WRITE) ? 1 : 0, // 0 means read
		vma ? ((vma->vm_flags & VM_READ) ? 1 : 0) : -1,
		vma ? ((vma->vm_flags & VM_WRITE) ? 1 : 0) : -1,
		vma ? (vma_is_anonymous(vma) ? 1 : 0) : -1,
		pte ? ((unsigned long)pte_flags(*pte) & 0xfff) : 0xfff,
		pte ? ((unsigned long)pte->pte >> PAGE_SHIFT) & 0xffffff : 0x000000,
		error_code, address,
		vma ? vma->vm_start : 0,
		vma ? vma->vm_end : 0,
		vma ? vma->vm_flags : 0,
		tsk->mm ? tsk->mm->start_stack : 0);
	dump_stack();
}

static pte_t *find_pte(unsigned long address)
{
	unsigned int level;
	return lookup_address(address, &level);
}

static int bad_address(void *p)
{
	unsigned long dummy;
	return probe_kernel_address((unsigned long *)p, dummy);
}

pte_t *find_pte_target(struct mm_struct *mm, unsigned long address)
{
	pgd_t *pgd = pgd_offset(mm, address);
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte = NULL;

	if (!pgd || (bad_address(pgd)) || pgd_none(*pgd))
		return NULL;
	p4d = p4d_offset(pgd, address);
	barrier();

	if (!p4d || (bad_address(p4d)) || p4d_none(*p4d))
		return NULL;
	pud = pud_offset(p4d, address);
	barrier();

	if (!pud || (bad_address(pud)) || pud_none(*pud))
		return NULL;
	pmd = pmd_offset(pud, address);
	barrier();

	if (pmd && likely(!bad_address(pmd)) && !pmd_none(*pmd))
	{
		pte = pte_offset_map(pmd, address);
		barrier();
		if ((bad_address(pte)))
			return NULL;
	}
	return pte;
}

/* We assume that it will be called only during the page fault handling */
pte_t *find_pte_target_lock(struct mm_struct *mm, unsigned long address, spinlock_t **ptl_ptr)
{
	pgd_t *pgd = pgd_offset(mm, address);
	p4d_t *p4d = NULL;
	pud_t *pud = NULL;
	pmd_t *pmd = NULL;
	pte_t *pte = NULL;
	*ptl_ptr = NULL;

	if (!pgd || pgd_none(*pgd))
		return NULL;
	p4d = p4d_offset(pgd, address);
	if (!p4d || p4d_none(*p4d))
		return NULL;
	pud = pud_offset(p4d, address);
	if (!pud || pud_none(*pud))
		return NULL;
	pmd = pmd_offset(pud, address);
	if (!pmd || pmd_none(*pmd))
		 return NULL;

	*ptl_ptr = pte_lockptr(mm, pmd);
	pte = pte_offset_map(pmd, address);
	return pte;
}

__always_inline static pte_t *get_pte_and_pte_lock(struct mm_struct *mm, unsigned long address,
	spinlock_t **ptl_ptr)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	// 1) Find pmd
	// See how to allocate pgd, pud, pmd @ memory.c / __handle_mm_fault()
	pgd = pgd_offset(mm, address);
	p4d = p4d_alloc(mm, pgd, address);
	if (!p4d)
		return NULL;

	pud = pud_alloc(mm, p4d, address);
	barrier();
	if (!pud)
		return NULL;
	pmd = pmd_alloc(mm, pud, address);
	barrier();
	if (!pmd)
		return NULL;

	// 2) Check pte
	// Maybe there is pte but not presented
	pte = find_pte(address);
	// Now we need page aligned address
	address &= PAGE_MASK;
	if (pte){
		goto return_pte;
	}
	
	// 3) Allocate pte
	if (pte_alloc(mm, pmd, address)){
		return NULL;
	}

return_pte:
	// PP_EXIT(FH_PTE_ALLOC_PTE);
	pte = pte_offset_map(pmd, address);
	// barrier();
	*ptl_ptr = pte_lockptr(mm, pmd);
	return pte;
}

extern int find_vma_links(struct mm_struct *mm, unsigned long addr,
		unsigned long end, struct vm_area_struct **pprev,
		struct rb_node ***rb_link, struct rb_node **rb_parent);

// Returns 0 on success. Returns 1 if the function failed (due to no free cnpages.
// After it eventually returns 0, please check the return arguments to see if what it returned
// makes sense.
static int prepare_data_page(
	unsigned int tgid, struct mm_struct *mm,
	pte_t *ptep,
	spinlock_t *pte_lock,
	unsigned long address,
	struct vm_area_struct **_vma,
	// Return arguments
	struct cnthread_page **cnpage_out,
	int *existing_page_out,
	bool *hold_vma_rlock_when_return_out)
{
	struct vm_area_struct *vma = *_vma;
	struct cnthread_page *cnpage = NULL;
	int free_list_empty;

	address &= PAGE_MASK;

	// TODO: eventually, we will not rely on VMA structure at all
	if (unlikely(!vma && tgid != DISAGG_KERN_TGID)) {
		pr_err("SHOOP: entered legacy branch alpha, remove this branch\n");
		BUG();
	}

	/*
	 *	Here we prepare page only for unpresented pte
	 */
	if (!pte_present(*ptep)) // read errors
	{
		// PP_INCREMENT(FH_pf_not_present_count);
		pr_pgfault("\tpte not present addr: %lx, pte: %llx, pte_val: %lx, mm: %llx\n",
				address, (u64)ptep, pte_val(*ptep), (u64)mm);

		free_list_empty = get_new_cnpage(tgid, address, mm, &cnpage, existing_page_out);
		if (free_list_empty) // LRU list empty!
			 return 1;

		if (!cnpage) {
			pr_err_ratelimited("SHOOP: cnthread_get_new_page returned NULL! how strange!\n");
			goto under_eviction;
		}

		if (unlikely(!cnpage->kpage)) {
			printk(KERN_DEFAULT "Cannot allocate new page at 0x%lx\n", address);
			BUG();
		}

		*cnpage_out = cnpage;
		return 0;
	}
	else if (pte_present(*ptep) && !pte_write(*ptep))
	{
		// PP_INCREMENT(FH_pf_present_count);

		pr_pgfault("\tpte present & not writable "
				"addr: %lx, pte: %llx, pte_val: %lx, mm: %llx\n",
				address, (u64)ptep, pte_val(*ptep), (u64)mm);
		free_list_empty = get_cnpage(tgid, address, mm, &cnpage, existing_page_out); // get existing or new
		if (free_list_empty)
			 return 1;

		if (!cnpage)
			goto under_eviction;

		*cnpage_out = cnpage;
		return 0;
	} else {
		BUG(); // Present & writable, then why it generate page fault?
	}

	return 0;

under_eviction:
	pr_pgfault("\tpage[%lx] under eviction or invalidation\n", address);
	*cnpage_out = NULL;
	return 0;
}

static pte_t *restore_data_page(
	struct task_struct *cur_tsk,
	pte_t *ptep, // PTE of address we faulted on
	spinlock_t *pte_lock,
	unsigned long address, // address we faulted on
	unsigned long vm_flags, // page fault "flags". IDK what this does.
	struct vm_area_struct *vma,  // VMA we faulted in
	struct cnthread_page *cnpage, // Relevant cnpage
	int existing_page) // 0 if we allocated the cnpage, 1 if we took it from the LRU.
{
	pte_t pte_val;
	struct page *page = NULL, *old_page = NULL;
	pgprot_t prot;
	void *old_data;
	int need_new_page = 0, err = 0;

	address &= PAGE_MASK;

	/*
	 *	Possible cases
	 *  1) new_page is not required (it was read-only but become write-able)
	 *  2) new_page is required and there is data to write (fetched from memory)
	 *  3) new_page is required but there is no data to write -> error
	 *  4) pte is already presented--it may already recovered by another threads -> skip
	 */

	// PHASE 1: Perform quick sanity checks, copy data from non-MIND pages to MIND pages as
	// needed.

	// Case 1: The old page table is read-only. Now, we're making it writeable.
	//         But the data itself is OK.
	if (pte_present(*ptep) && !pte_write(*ptep))
	{
		// Use the old page
		page = pte_page(*ptep);
		if (unlikely(page && atomic_read(&page->_refcount) > 3))
		{
			void *new_data;
			// Create new page if there are multiple mappings 
			// — THIS SHOULD NOT HAPPEN AS WE MANUALLY HANDLE FORK 
			//	 (only happens when copying data from non-remote process to remote process,
			//	  e.g., start the first remote process)
			BUG_ON(!cnpage);
			old_page = page;
			page = cnpage->kpage;
			BUG_ON(!page); // can't allocate new page at the addr...

			// TODO(yash): Aren't we just copying data from a cnpage to itself here?
			//             Or is this if the old mapping was to a non-MIND page?
			//             I wish there was an easy way to tell which was which. It
			//             would save me copies.
			old_data = kmap_atomic(old_page);
			new_data = kmap_atomic(cnpage->kpage);
			memcpy(new_data, old_data, PAGE_SIZE);
			kunmap_atomic(new_data);
			kunmap_atomic(old_data);

		}
	}

	// Case 2: The old page wasn't present. In this case, we just allocated a new one.
	else if (!pte_present(*ptep))
	{
		// If the page wasn't present...then what did we allocate?
		BUG_ON(existing_page);

		BUG_ON(!cnpage);
		page = cnpage->kpage;
		need_new_page = 1;
	}
	// Case 3: It was present and writeable...so why did it fault?
	else {
		BUG();
	}

	err = cnthread_add_pte_to_list_with_cnpage(ptep, pte_lock,
			address, vma, cnpage, !existing_page);
	if (unlikely(err))
		return NULL;

	smp_wmb();
	__set_bit(PG_uptodate, &page->flags);

	// Update mm for user process
	if (!is_kshmem_address(address)) {
		// TRY to skip flush cache (we are just adding empty page...)
		BUG_ON(check_stable_address_space(cur_tsk->mm));
		// XXX: why accounted as MM_FILEPAGES not MM_ANONPAGES?
		if (need_new_page)
			inc_mm_counter(cur_tsk->mm, MM_FILEPAGES);
	}

	prot = vm_get_page_prot(vm_flags);
	pte_val = mk_pte(page, prot);
	pte_val = pte_set_flags(pte_val, _PAGE_PRESENT);
	// Mark the PTE as writeable, regardless of whether it's read or write fault.
	pte_val = pte_mkwrite(pte_val);
	// Mark the PTE as recently accessed.
	pte_val = pte_mkyoung(pte_val);
	// Mark page as clean (XXX: I think this is true by default?)
	pte_val = pte_mkclean(pte_val);
	// pte_val = pte_mkold(pte_val);	// clear accessed bit
	if (is_kshmem_address(address))
		 pte_val = pte_clear_flags(pte_val, _PAGE_USER);
	smp_wmb();
	set_pte_at_notify(is_kshmem_address(address) ? &init_mm : cur_tsk->mm, address, ptep, pte_val);
	mmu_notifier_invalidate_range_only_end(is_kshmem_address(address) ? &init_mm : cur_tsk->mm,
										   address, address + PAGE_SIZE);

	if (old_page) {
		put_page(old_page);
		// it should be the page managed by original Linux kernel not MIND
		// put_cnpage(old_page);	
	}
	return ptep;
}

// Sync counter once per 64 page faults
#ifndef TASK_RSS_EVENTS_THRESH
#define TASK_RSS_EVENTS_THRESH	(64)
#endif

static void check_sync_rss_stat(struct task_struct *tsk)
{
	if (unlikely(tsk != current))
		return;
	if (tsk->mm && unlikely(tsk->rss_stat.events++ > TASK_RSS_EVENTS_THRESH))
		sync_mm_rss(tsk->mm);
}

static void patr_udelay(int duration_us)
{
	ktime_t start_time = ktime_get();
	ktime_t end_time = ktime_add_us(start_time, duration_us);

	while (ktime_before(ktime_get(), end_time)) {
		patr_poll_fh_queue();
		udelay(5); // TODO: remove :)
	}
	cond_resched(); // TODO do I still need this?
}

// TODO(yash): I think I can sleep here now).
static void sleep_for_random_duration(int back_off_cnt)
{
	unsigned long tmp_jiff = jiffies, target_wait_time = 0;
	unsigned char randval = 0;

	might_sleep();

	// Back-off routine
	get_random_bytes(&randval, 1);
	target_wait_time = (DISAGG_NACK_RETRY_IN_USEC) + (back_off_cnt * (DISAGG_NACK_RETRY_IN_USEC * (unsigned int)randval) / 256);

	tmp_jiff = jiffies_to_usecs(jiffies - tmp_jiff);
	if ((tmp_jiff >= 0) && (tmp_jiff < DISAGG_NACK_RETRY_IN_USEC * back_off_cnt))
		patr_udelay(DISAGG_NACK_RETRY_IN_USEC * back_off_cnt - tmp_jiff);
	else if (tmp_jiff == 0)
		patr_udelay(DISAGG_NACK_RETRY_IN_USEC * back_off_cnt);
}

enum {
	FH_ALREADY_EVICT = 1,
	FH_NACK_NORMAL = 2,
	FH_NACK_FROM_RESTORE = 3,
	FH_ALREADY_EVICT_WAIT = 10,
};


void do_disagg_page_fault(struct task_struct *tsk, struct pt_regs *regs,
		unsigned long error_code, unsigned long address, unsigned int flags)
{
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *vma;
	int fault, major = 0;
	u32 pkey;
	int retrieved_cnpage_from_lru = 0;
	int retry_after_backoff = 0;
	int return_code = 0;
	unsigned int backoff_count = 1;
	spinlock_t *pte_lock = NULL;
	struct cnpage_lock pgfault_lock;
	int is_kern_shared_mem = 0;
	// Why track lock status ourself? because this function reuses error error code, and
	// relying on `is_spin_locked()` might unlock a _different_ page faulter's PTE.
	bool mmap_sem_is_locked;
	bool wait_for_free_page = false;

	PP_STATE(FH_total);
	// PP_STATE(FH_init_vma);
	// PP_STATE(FH_get_pte);
	// PP_STATE(FH_prepare_page);
	// PP_STATE(FH_restore_data_page);
	// PP_STATE(FH_pdp);
	// PP_STATE(FH_wait_for_free_page);
	// _PP_TIME(FH_wait_for_free_page) = -1; // make compiler happy (uninitialized var warning).

	PP_ENTER(FH_total);

	// y: This silences an uninitialized variable warning.
	// __profilepoint_start_ns_FH_wait_for_free_page = 0;

	if (tsk->is_mind_binary) {
		char const *debug_msg;
		if (error_code & X86_PF_USER)
			 debug_msg = "from userspace";
		else
			 debug_msg = "";
		pr_pgfault("BEGIN PFAULT %s: tgid[%d] pid[%d] ip[%lx] addr[%lx] errcode[%lx] CPU[%d]\n",
				debug_msg, tsk->tgid, tsk->pid, regs->ip, address, error_code, smp_processor_id());
	}

	// we need to use kernel's mm for kernel shared memory
	if (is_kshmem_address(address)) {
		is_kern_shared_mem = 1;
		if (!mm)
			 mm = &init_mm;
	}

retry:
	// PP_ENTER(FH_init_vma);
	/*
	 * When running in the kernel we expect faults to occur only to
	 * addresses in user space.  All other faults represent errors in
	 * the kernel and should generate an OOPS.  Unfortunately, in the
	 * case of an erroneous fault occurring in a code path which already
	 * holds mmap_sem we will deadlock attempting to validate the fault
	 * against the address space.  Luckily the kernel only validly
	 * references user space from well defined areas of code, which are
	 * listed in the exceptions table.
	 *
	 * As the vast majority of faults will be valid we will only perform
	 * the source reference check when there is a possibility of a
	 * deadlock. Attempt to lock the address space, if we cannot we then
	 * validate the source. If this is invalid we can skip the address
	 * space check, thus avoiding the deadlock:
	 */
	if (unlikely(!down_read_trylock(&mm->mmap_sem)))
	{
		if (!(error_code & X86_PF_USER) &&
				!search_exception_tables(regs->ip) && !is_kern_shared_mem) {
			bad_area_nosemaphore(regs, error_code, address, NULL);
			return;
		}
		down_read(&mm->mmap_sem);
	}
	// The above down_read_trylock() might have succeeded in which case we'll
	// have missed the might_sleep() from down_read().
	might_sleep();

	// ======= CUSTOM PAGE FAULT ROUTINE ====== //
	// Initial values. Leave them here, in case of `goto retry;`
	major = 0;
	retrieved_cnpage_from_lru = 0;
	retry_after_backoff = 0;
	mmap_sem_is_locked = true;

	/*
	 * Try to get vma, filter out bad page fault
	 */
	if (is_kern_shared_mem) {
		vma = NULL;
	} else {
		BUG_ON(address == 0);
		vma = find_vma(mm, address); // current or next vma
		pr_pgfault("PFAULT: find_vma: addr: 0x%lx, vma: 0x%lx - 0x%lx\n", address,
				vma ? vma->vm_start : 0, vma ? vma->vm_end : 0);
	}

	if (error_code & X86_PF_INSTR)
		 goto normal_linux_routine;

	// VMA: the first vma of which the vm_end > address
	if (!vma || (vma && (vma->vm_start > address)) // normal case (e.g., newly mapped mmaps)
			|| (vma_is_anonymous(vma) && !(vma->vm_file) &&
				// WRITE permission
				(((error_code & X86_PF_WRITE) && !(vma->vm_flags & VM_WRITE)) ||
				 // READ permission
				 (!(error_code & X86_PF_WRITE) && !(vma->vm_flags & VM_READ))))
	   )
	{
		struct cnthread_page *cnpage = NULL;
		int free_list_empty;
		pte_t *ptep;
		// PP_STATE(FH_rangelock);
		// PP_STATE(FH_cleanup);

		if (!vma || (vma->vm_start > address))
			 vma = NULL;

		// PP_EXIT(FH_init_vma);

		// PHASE 1: Lock the address region. Obtain and lock the PTE.

		// PP_ENTER(FH_rangelock);
		pr_locks("FH:l:%lx", address >> PAGE_SHIFT);
		cnpage_lock_range__patr_delay(is_kern_shared_mem ? DISAGG_KERN_TGID : tsk->tgid,
				(address & PAGE_MASK), (address & PAGE_MASK) + PAGE_SIZE, &pgfault_lock);
		// PP_EXIT(FH_rangelock);

		// PP_ENTER(FH_get_pte);
		pte_lock = NULL;
		ptep = get_pte_and_pte_lock(mm, (address & PAGE_MASK), &pte_lock);
		// Could we get PTE?
		BUG_ON(!ptep || !pte_lock);
		// Are the CR3 register and the page table pointing to the same PTE?
		BUG_ON(ptep != find_pte_from_reg(address));

		spin_lock(pte_lock);
		// PP_EXIT(FH_get_pte);

		// Check: Did another thread already solve this?
		if (  (!(error_code & X86_PF_WRITE) && pte_present(*ptep)) // read request
		    || ((error_code & X86_PF_WRITE) && pte_present(*ptep) && pte_write(*ptep))) { // write request
			spin_unlock(pte_lock);
			retry_after_backoff = 0;
			goto back_off;
		}

		// PHASE 2: Prepare a page for us to RDMA into.

		// if (unlikely(wait_for_free_page))
			 // PP_EXIT(FH_wait_for_free_page);

		// PP_ENTER(FH_prepare_page);
		// PP_ENTER(FH_pdp);
		free_list_empty = prepare_data_page(is_kern_shared_mem ? DISAGG_KERN_TGID : tsk->tgid,
				mm, ptep, pte_lock,  address, &vma, &cnpage,
				&retrieved_cnpage_from_lru, &mmap_sem_is_locked);
		// PP_EXIT(FH_pdp);
		spin_unlock(pte_lock);

		if (unlikely(free_list_empty)) { // TODO: After I've fixed the try_lock issue, I can just
				                 // drop the PTE, sleep, and retry. No need to drop range
				                 // lock.
			// PP_ENTER(FH_wait_for_free_page);
			wait_for_free_page = true;

			cnpage_unlock_range(is_kern_shared_mem ? DISAGG_KERN_TGID : tsk->tgid, &pgfault_lock);
			if (mmap_sem_is_locked)
				 up_read(&mm->mmap_sem);
			pr_pgfault("PFAULT[cpu %d]: Backing off due to empty freelist\n",
					smp_processor_id());
			patr_udelay(300);
			goto retry;
		}
		if (!cnpage) { // During eviction. TODO might not be accurate.
			return_code = FH_ALREADY_EVICT;
			goto back_off;
		}

		// PP_EXIT(FH_prepare_page);

		// PHASE 3: Fetch data from Memory Node.

		// We couldn't find existing data for this page. So let's fetch it into our new
		// cnpage.
		if (!retrieved_cnpage_from_lru) {
			// PP_STATE(FH_rdma);
			// PP_ENTER(FH_rdma);
			send_page_fault_to_memory(tsk, (address & PAGE_MASK), cnpage->dma_addr);
			// PP_EXIT(FH_rdma);
			fault = DISAGG_FAULT_READ;
			set_cnpage_received(cnpage); // Mark page as received (but not yet used).
		}

		// Phase 4: Restore the data page
		// PP_ENTER(FH_restore_data_page);

		flags = (flags & 0xF) & ~VM_WRITE;
		check_sync_rss_stat(tsk);

		// Extract data from memory reply
		// XXX(yash): shouldn't we lock the PTE during this operation? It sets PTE bits...
		if (!restore_data_page(tsk, ptep, pte_lock, address, flags, vma, cnpage,
					retrieved_cnpage_from_lru))
		{
			if (!retrieved_cnpage_from_lru)	// Was not existing
			{
				pr_pgfault("PFAULT: CN [%d]: put_page (3) 0x%lx\n", smp_processor_id(), address);
				BUG_ON(atomic_read(&cnpage->is_used) != CNPAGE_IS_RECEIVED);
				put_cnpage(cnpage);
			}
			retry_after_backoff = 0;
			return_code = FH_NACK_FROM_RESTORE;
			goto back_off;
		}
		// PP_EXIT(FH_restore_data_page);
		// PP_ENTER(FH_cleanup);

		// BUG_ON(!pte_present(*ptep));
		// WARN_ON_ONCE(!pte_write(*ptep));
		// WARN_ON_ONCE(pte_write(*ptep) && pte_dirty(*ptep));
		// WARN_ON_ONCE(pte_write(*ptep) && pte_soft_dirty(*ptep));
		// WARN_ON_ONCE(PageDirty(cnpage->kpage));

		cnpage_unlock_range(is_kern_shared_mem ? DISAGG_KERN_TGID : tsk->tgid, &pgfault_lock);
		up_read(&mm->mmap_sem); 
		// PP_EXIT(FH_cleanup);

		PP_EXIT(FH_total);
		pr_pgfault("END PFAULT: tgid[%d] pid[%d] ip[%lx] addr[%lx] errcode[%lx] CPU[%d]\n",
				tsk->tgid, tsk->pid, regs->ip, address, error_code, smp_processor_id());
		return;
	}


	// ====== normal Linux routine ===== //
normal_linux_routine:
	// PP_INCREMENT(FH_linux_fh_);

	if (unlikely(!vma)) {
		goto bad_area;
	}
	if (likely(vma->vm_start <= address))
		 goto good_area;
	if (unlikely(!(vma->vm_flags & VM_GROWSDOWN))) {
		goto bad_area;
	}
	if (error_code & X86_PF_USER) {
		/*
		 * Accessing the stack below %sp is always a bug.
		 * The large cushion allows instructions like enter
		 * and pusha to work. ("enter $65535, $31" pushes
		 * 32 pointers and then decrements %sp by 65535.)
		 */
		if (unlikely(address + 65536 + 32 * sizeof(unsigned long) < regs->sp)) {
			goto bad_area;
		}
	}
	if (unlikely(expand_stack(vma, address))) {
		goto bad_area;
	}
	printk(KERN_DEFAULT "CN: stack expanded - addr: 0x%lx, vma: 0x%lx - 0x%lx [0x%lx]\n",
			address, vma->vm_start, vma->vm_end, vma->vm_flags);
	goto good_area;

bad_area:
	if (vma) {
		pr_pgfault("CN: BAD AREA- addr: 0x%lx, vma: 0x%lx - 0x%lx [0x%lx]\n",
				address, vma->vm_start, vma->vm_end, vma->vm_flags);
	}
	print_pgfault_error(tsk, error_code, address, vma, 0);

	/// this fn releases mmap_sem
	bad_area(regs, error_code, address);
	return;

	/*
	 * Ok, we have a good vm_area for this memory access, so
	 * we can handle it..
	 */
good_area:
	if (unlikely(access_error(error_code, vma))) {
		printk(KERN_DEFAULT "CN: GOOD AREA- addr: 0x%lx, vma: 0x%lx - 0x%lx [0x%lx]\n",
				address, vma->vm_start, vma->vm_end, vma->vm_flags);
		print_pgfault_error(tsk, error_code, address, vma, 0);
		DEBUG_print_vma(mm);
		barrier();

		bad_area_access_error(regs, error_code, address, vma);
		return;
	}

	/*
	 * If for any reason at all we couldn't handle the fault,
	 * make sure we exit gracefully rather than endlessly redo
	 * the fault.  Since we never set FAULT_FLAG_RETRY_NOWAIT, if
	 * we get VM_FAULT_RETRY back, the mmap_sem has been unlocked.
	 *
	 * Note that handle_userfault() may also release and reacquire mmap_sem
	 * (and not return with VM_FAULT_RETRY), when returning to userland to
	 * repeat the page fault later with a VM_FAULT_NOPAGE retval
	 * (potentially after handling any pending signal during the return to
	 * userland). The return to userland is identified whenever
	 * FAULT_FLAG_USER|FAULT_FLAG_KILLABLE are both set in flags.
	 * Thus we have to be careful about not touching vma after handling the
	 * fault, so we read the pkey beforehand.
	 */
	pkey = vma_pkey(vma);
	fault = handle_mm_fault(vma, address, flags);
	major |= fault & VM_FAULT_MAJOR;

	/*
	 * If we need to retry the mmap_sem has already been released,
	 * and if there is a fatal signal pending there is no guarantee
	 * that we made any progress. Handle this case first.
	 */
	if (unlikely(fault & VM_FAULT_RETRY)) {
		/* Retry at most once */
		if (flags & FAULT_FLAG_ALLOW_RETRY) {
			flags &= ~FAULT_FLAG_ALLOW_RETRY;
			flags |= FAULT_FLAG_TRIED;
			if (!fatal_signal_pending(tsk))
				 goto retry;
		}

		/* User mode? Just return to handle the fatal exception */
		if (flags & FAULT_FLAG_USER)
			 return;

		/* Not returning to user mode? Handle exceptions or die: */
		no_context(regs, error_code, address, SIGBUS, BUS_ADRERR);
		return;
	}

	up_read(&mm->mmap_sem);
	if (unlikely(fault & VM_FAULT_ERROR)) {
		mm_fault_error(regs, error_code, address, &pkey, fault);
		return;
	}
	// check_v8086_mode(regs, address, tsk);    // ignore this for 64-bit
	return;


back_off:
	// PP_INCREMENT(FH_give_up_count);

	cnpage_unlock_range(is_kern_shared_mem ? DISAGG_KERN_TGID : tsk->tgid, &pgfault_lock);

	pr_pgfault("PFAULT: return and retry addr[%lx] ret_code[%d]\n", address, return_code);
	if (mmap_sem_is_locked)
		 up_read(&mm->mmap_sem);

	if (retry_after_backoff) {
		backoff_count++;
		if (backoff_count >= DISAGG_NACK_MAX_BACKOFF)
			 backoff_count = DISAGG_NACK_MAX_BACKOFF;
		sleep_for_random_duration(backoff_count);
		goto retry;
	}
	return;
}
NOKPROBE_SYMBOL(do_disagg_page_fault);


/* vim: set sw=8 ts=8 noexpandtab tw=99 */
