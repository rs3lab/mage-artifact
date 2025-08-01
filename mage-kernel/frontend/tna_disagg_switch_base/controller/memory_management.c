#include "controller.h"
#include "memory_management.h"
#include "request_handler.h"
#include "rbtree_ftns.h"
#include "cacheline_manager.h"
#include "cache_manager_thread.h"
#include "cacheline_def.h"
#include "config.h"
#include "pid_ns.h"
#include "kshmem.h"
#include <stdio.h>

static unsigned long get_smaller_cache_region_index(unsigned long index) {
	switch (index) {
	case REGION_SIZE_16KB:
		return REGION_SIZE_4KB;
	case REGION_SIZE_64KB:
		return REGION_SIZE_16KB;
	case REGION_SIZE_2MB:
		return REGION_SIZE_64KB;
	default:
		return 0;
	}
}

//duplicate from task_management.c
static inline u64 generate_full_addr(u16 tgid, unsigned long addr)
{
    u64 full_addr = (u64)tgid << MN_VA_PID_SHIFT; // First 16 bits
    full_addr += addr & (~MN_VA_PID_BIT_MASK);    // Last 48 bits
    return full_addr;
}

static int mn_common_new_task_mm(
	u16 sender_id, u16 tgid, u16 pid_ns_id, u16 pid, struct task_struct *old_tsk)
{
	// Old_tsk can be NULL to generate dummy structures
	struct task_struct *tsk;
	int res = -1;
	int i;

	// Generate dummy task struct
	tsk = (struct task_struct*)malloc(sizeof(*tsk));
	if (!tsk)
	{
		fprintf(stderr, "Cannot allocate new task");
		goto new_task_mm_out;
	}

	// Initialize generated task with new mm
	tsk->tgid = tgid;
	tsk->pid = pid;
	tsk->pid_ns_id = pid_ns_id;
	tsk->primary_node = sender_id;

	for (i = 0; i < MAX_NUMBER_COMPUTE_NODE + 1; i++)
	{
		tsk->ref_cnt[i] = 0;
	}
	tsk->ref_cnt[sender_id] = 1;

	for (i = 0; i < MAX_NUMBER_COMPUTE_NODE + 1; i++)
	{
		tsk->tgids[i] = 0;
	}
	if (tgid)
		tsk->tgids[sender_id] = tgid;
	else
		tsk->tgids[sender_id] = 1;	// FIXME: just counter

	// y: Copy the MM. We also derive the new MM's addr here.
	tsk->mm = mn_dup_mm(tsk, old_tsk, sender_id);
	printf("New task: sender[%d], tgid[%d]\n", sender_id, tgid);
	if (!tsk->mm)
	{
		fprintf(stderr, "Cannot allocate new mm");
		free(tsk);
		goto new_task_mm_out;
	}

	// Put the task struct
	res = mn_insert_new_task_mm(sender_id, tgid, pid_ns_id, tsk);

new_task_mm_out:
	return res;
}

// Memory management used in handlers
int mn_create_dummy_task_mm(u16 sender_id, u16 tgid, u16 pid_ns_id, u16 pid)
{
	return mn_common_new_task_mm(sender_id, tgid, pid_ns_id, pid, NULL);
}

int mn_create_mm_from(u16 sender_id, u16 tgid, u16 pid_ns_id, u16 pid, struct task_struct *old_tsk, u32 clone_flags)
{
	int ret = -1;
	(void)clone_flags;	// not used for now
	ret = mn_common_new_task_mm(sender_id, tgid, pid_ns_id, pid, old_tsk);
	return ret;
}


// From EXEC handler
int mn_update_mm(u16 sender_id, u16 tgid, u16 pid_ns_id, struct exec_msg_struct* exec_req)
{
	int ret = -1;
	struct task_struct *tsk = mn_get_task(tgid);

	//    if(!tsk && (tsk->tgid != TEST_PROGRAM_TGID))
	if(!tsk && !contains_tgid_ns(tgid, pid_ns_id))
		 return -1;	// must be forked first
	if(contains_tgid_ns(tgid, pid_ns_id) && !tsk)
	{
		//FIXME ignore this return value for now
		mn_create_dummy_task_mm(sender_id, tgid, pid_ns_id, exec_req->pid);
		printf("Dummy task/mm inserted in exec): sender: %u, tgid: %u, pid: %u\n",
				(unsigned int)sender_id, (unsigned int)tgid,
				(unsigned int)exec_req->pid);
		tsk = mn_get_task(tgid);
	}

	// erase existing vmas and rb-tree
	if (sem_wait(&tsk->mm->mmap_sem)) {
		return -EINTR;
	}
	mn_remove_vmas_exec(tsk->mm);
	sem_post(&tsk->mm->mmap_sem);

	// For debugging: print difference of mmap in mn and that in exec req
	// DEBUG_print_vma_diff(tsk->mm, exec_req);

	// copy vmas and rb-tree
	// spin_lock(&tsk->alloc_lock);
	if(contains_tgid_ns(tsk->tgid, pid_ns_id) && tsk)
		 ret = mn_build_mmap_from_exec_with_cache(tsk->mm, exec_req, tsk, sender_id);
	else
		 ret = mn_build_mmap_from_exec(tsk->mm, exec_req);
	// spin_unlock(&tsk->alloc_lock);

	// For debugging: print the result--vma list
	// DEBUG_print_vma(tsk->mm);
	return ret;
}

int mn_check_vma(u16 sender_id, u16 tgid, u16 pid_ns_id, struct exec_msg_struct* exec_req)
{
	struct task_struct *tsk = mn_get_task(tgid);

	// If there is no existing entry, make a dummy one
    if (tsk)
    {
		(void)exec_req;
	}
	else
	{
		fprintf(stderr, "VMA_CHECK - Cannot find task_struct: %u:%u\n",
				    sender_id, tgid);
	}
	return 0;
}

static int is_test_vma_req(struct task_struct *tsk, unsigned long len)
{
	if (tsk && contains_tgid_ns(tsk->tgid, tsk->pid_ns_id) &&
		(len == TEST_INIT_ALLOC_SIZE || len == TEST_MACRO_ALLOC_SIZE))
		return 1;

	return 0;
}

static int is_test_meta_vma_req(struct task_struct *tsk, unsigned long len)
{
	if (tsk && contains_tgid_ns(tsk->tgid, tsk->pid_ns_id) && len == TEST_META_ALLOC_SIZE)
		return 1;

	return 0;
}

int alloc_fine_grained_cache(u64 addr) {
#ifdef TEST_LOCK_MICRO_BENCH
	return ((addr >= TEST_MEM_ACC_ADDR && addr < TEST_TLS_ADDR)
		|| (addr >= kshmem_get_va_start())) ? 1 : 0;
#else
	return 0;
#endif
}

int mn_populate_cache(u16 tgid, u16 pid_ns_id, u16 pid, unsigned long addr, unsigned long len, int nid,
					  unsigned long flags, vm_flags_t vm_flags, unsigned long *file)	// just for printing
{
	unsigned long tmp_addr = addr;
	int recheck_free_entry_cnt = 0;
	int num_entry_free;
	int num_entry_needed = min(len, TEST_DEBUG_SIZE_LIMIT)/ INITIAL_REGION_SIZE
						   + (len - min(len, TEST_DEBUG_SIZE_LIMIT)) / CACHELINE_MAX_SIZE;
	int populated = 1;

#ifndef CACHE_DIR_PRE_POP_IDLE
	int state = CACHELINE_SHARED;
#else
	int state = CACHELINE_IDLE;
#endif
#ifdef CACHE_OWNERSHIP_OPT
	state = CACHELINE_MODIFIED;
#endif
	// this would be just approximate after we add alignment problem
	printf("MMAP-Cacheline Init: tgid: %d, pid: %d, addr: 0x%lx - 0x%lx, vmflag: 0x%lx, flag: 0x%lx, file: %lu\n",
		   (int)tgid, (int)pid, tmp_addr, tmp_addr + len,
		   vm_flags, flags, (unsigned long)file);
	// request unlock before locking
	cacheman_request_unlock();
recheck_free_entry:
	cacheman_run_lock();	// To prevent confliction with cache manager thread
	// in case we run out of entry for merge
	if ((num_entry_free = get_free_dir_cnt()) < DYN_CACHE_MIN_DIR_FOR_MMAP + num_entry_needed) {
		cacheman_run_unlock();
		if ((++recheck_free_entry_cnt) >= DYN_CACHE_MMAP_MAX_RETRY) {
			printf("No enough entry: free[%d] need[%d]\n", num_entry_free, num_entry_needed);
			// TODO: let it fail below, just for now...
		} else {
			printf("Low entry: free[%d] need[%d], wait...\n", num_entry_free, num_entry_needed);
			usleep(DYN_CACHE_SLEEP_US_IF_NO_DIR);
			goto recheck_free_entry;
		}
	}

	// 16 KB for the first TEST_DEBUG_SIZE_LIMIT entries
	int i;
	unsigned long index, size;
#ifdef TEST_LOCK_MICRO_BENCH
	int is_alloc_fine_grained_cache = alloc_fine_grained_cache(addr);
	if (is_alloc_fine_grained_cache) {
		printf("MICRO BENCH: alloc fine grained cache for addr: 0x%lx - 0x%lx\n", addr, addr + min(len, TEST_DEBUG_SIZE_LIMIT));
	}
#endif
	for (; tmp_addr < addr + min(len, TEST_DEBUG_SIZE_LIMIT);)
	{
#ifdef TEST_LOCK_MICRO_BENCH
		if (is_alloc_fine_grained_cache) {
			index = REGION_SIZE_4KB;
			size = (1 << (index + REGION_SIZE_BASE));
		} else
#endif
		{
			if (tgid == DISAGG_KERN_TGID)
			{
				// use smaller pages for the kernel shared memory
				index = MINIMAL_REGION_INDEX;
				size = MINIMAL_REGION_SIZE;

			} else {
				index = INITIAL_REGION_INDEX;
				size = INITIAL_REGION_SIZE;
			}
			while (tmp_addr % size || (tmp_addr + size > addr + min(len, TEST_DEBUG_SIZE_LIMIT))) {
				// printf("tmp_addr: %lx, size: %lx, limit: %lx\n", tmp_addr, size, addr + min(len, TEST_DEBUG_SIZE_LIMIT));
				index = get_smaller_cache_region_index(index);
				size = (1 << (index + REGION_SIZE_BASE));
			}
		}

		if (!create_new_cache_dir(get_full_virtual_address(tgid, tmp_addr),
					state, nid, index))
		{
			printf("MMAP-Cacheline Init Err: tgid: %d, pid: %d, addr: 0x%lx - 0x%lx, vmflag: 0x%lx, flag: 0x%lx, file: %lu\n",
			(int)tgid, (int)pid, tmp_addr, tmp_addr + size,
			vm_flags, flags, (unsigned long)file);
			populated = 0;
			break;
		}
		tmp_addr += size;
	}
	// 2 MB regions for the remaining ranges
	if (populated)
	{
		for (; tmp_addr < addr + len; tmp_addr += CACHELINE_MAX_SIZE)
		{
			if (!create_new_cache_dir(get_full_virtual_address(tgid, tmp_addr),
						state, nid, REGION_SIZE_TOTAL))
			{
				printf("MMAP-Cacheline Init Err: tgid: %d, pid: %d, addr: 0x%lx - 0x%lx, vmflag: 0x%lx, flag: 0x%lx, file: %lu\n",
					(int)tgid, (int)pid, tmp_addr, tmp_addr + CACHELINE_MAX_SIZE,
					vm_flags, flags, (unsigned long)file);
				populated = 0;
				break;
			}
		}
	}
//for debug
// 			if (addr == 0x1cda000) {
// 				unsigned long debug_addr;
// 				for (debug_addr = addr; debug_addr < addr + len; debug_addr += CACHELINE_MIN_SIZE) {
// 					read_cache_dir_for_test(tsk->tgid, debug_addr, 0);
// 				}
// 			}
	cacheman_run_unlock();
	return populated;
}

unsigned long mm_do_mmap(
		struct task_struct *tsk,
		unsigned long addr,  // mmap.argv[0]: addr we're trying to mmap to
		unsigned long len,   // mmap.argv[1]: how many bytes we're trying to map
		unsigned long prot,  // mmap.argv[2]: args
		unsigned long flags, // mmap.argv[3]: MAP_SHARED, MAP_PRIVATE, etc
		vm_flags_t vm_flags,
		unsigned long pgoff, // offset to read from file
		unsigned long *file, // file
		int nid,             // nid of the caller of mmap
		int need_cache_entry)
{
	struct mm_struct *mm = tsk->mm;	// we just use mm from tsk

	if (!mm)
		 return -EINVAL;

	// Check testing VMA
	sem_wait(&mm->mmap_sem);

	// Map a cache-line aligned area.
	if (len % CACHELINE_MAX_SIZE)
		len = ((len / (unsigned long)CACHELINE_MAX_SIZE) + 1) * CACHELINE_MAX_SIZE;	

	//is_test discarded in multithreading!!!
	addr = _mn_do_mmap(tsk, addr, len, prot, flags, vm_flags, pgoff, file, 0);

	// skip cache entry init when not necessary
	if (!need_cache_entry)
		goto out;

#ifdef CACHE_DIR_PRE_OPT
	if (!IS_ERR_VALUE(addr) && !file)
	{
		// for now, just give all permission to ano mappings
		if (mn_populate_cache(tsk->tgid, tsk->pid_ns_id, tsk->pid, addr, len, nid, flags, vm_flags, file))	// try to populate cachelines
		{
			addr |= MMAP_CACHE_DIR_POPULATION_FLAG;
			usleep(1000); // 1 ms
			barrier();
		}
	}
#endif

out:
	sem_post(&mm->mmap_sem);
	pr_vma("done mn_do_mmap\n");
	return addr;
}

int send_remote_munmap_to_cn(struct munmap_msg_struct *req, struct socket *sk)
{
    int ret;
    char retbuf[64];
    //struct remotethread_reply_struct *ret_buf_ptr;
    if (!sk)
    {
        printf("remote munmap - can not get the socket to send requests\n");
    } else {
        printf("sk and tsk good\n");
    }

    if (!req) {
        printf("no munmap req to forward\n");
    } else {
        printf("munmap_req good\n");
    }

    memset(retbuf, 0, sizeof(retbuf));

    //send and recv remote thread msg
    size_t tot_size = sizeof(*req);

    ret = send_msg_to_cn(DISAGG_REMOTE_MUNMAP, sk, req, tot_size, retbuf, 64);

    // 0 means success
    if (ret >= 0)
    {
		ret = 0;
    } else {
        printf("fail to send munmap msg to cn, ret: %d\n", ret);
	}

    return ret;
}

int mn_do_munmap(struct task_struct *tsk, struct mm_struct *mm, unsigned long start, size_t len, struct munmap_msg_struct *req, int sender_id)
{
	int res;
	
	sem_wait(&mm->mmap_sem);

	// should we multicase munmap while holding the lock?
	for (int nid = 0; nid < MAX_NUMBER_COMPUTE_NODE + 1; ++nid) {
		if (nid != sender_id && tsk->tgids[nid]) {
			// printf("me[%d] send munmap msg to nid[%d] begin\n", sender_id, nid);
			send_remote_munmap_to_cn(req, get_remote_thread_socket(nid)->sk);
			// printf("me[%d] send munmap msg to nid[%d] end\n", sender_id, nid);
		}
	}

	res = mn_munmap(mm, start, len);

	cacheman_run_lock();
	unsigned long addr;
	for (addr = start; addr < start + PAGE_ALIGN(len); addr += CACHELINE_MIN_SIZE)
    {
        try_clear_cache_dir_with_boundry(generate_full_addr(tsk->tgid, addr), start + PAGE_ALIGN(len));
    }
	cacheman_run_unlock();
  	printf("MUNMAP-Cleared cache for [0x%lx - 0x%lx], len: %lx\n", start, start + len, len);

	sem_post(&mm->mmap_sem);

	if (is_test_vma_req(mm->owner, len))
	{
		mm->testing_vma.data_addr = 0;
	}
	else if (is_test_meta_vma_req(mm->owner, len))
	{
		mm->testing_vma.meta_addr = 0;
	}
	return res;
}

unsigned long mn_do_mremap(struct task_struct *tsk, unsigned long addr, unsigned long old_len,
			unsigned long new_len, unsigned long flags,	unsigned long new_addr)
{
	unsigned long res;

	sem_wait(&tsk->mm->mmap_sem);
	res = mn_mremap(tsk, addr, old_len, new_len, flags, new_addr);
	sem_post(&tsk->mm->mmap_sem);
	return res;
}

unsigned long mn_do_brk(struct task_struct *tsk, unsigned long brk, int nid)
{
	unsigned long newbrk, oldbrk;
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *next_vma;
	unsigned long min_brk;
	struct munmap_msg_struct tmp_req;

	sem_wait(&mm->mmap_sem);

	min_brk = mm->start_brk;
	if (brk < min_brk)
		goto out;

	printf("BRK: addr: 0x%lx, mm->brk: 0x%lx\n", brk, mm->brk);

	/*
	 * Check against rlimit here. If this check is done later after the test
	 * of oldbrk with newbrk then it can escape the test and let the data
	 * segment grow beyond its set limit the in case where the limit is
	 * not page aligned -Ram Gupta
	 */
	newbrk = PAGE_ALIGN(brk);
	oldbrk = PAGE_ALIGN(mm->brk);
	if (oldbrk == newbrk)
		goto set_brk;

	/* Always allow shrinking brk. */
	if (brk <= mm->brk) {
		if (!mn_munmap(mm, newbrk, oldbrk-newbrk))
			goto set_brk;
		goto out;
	}

	/* Check against existing mmap mappings. */
	next_vma = mn_find_vma(mm, oldbrk);
	if (next_vma && newbrk + PAGE_SIZE > vm_start_gap(next_vma))
		goto out;

	/* Ok, looks good - let it rip. */
	if (mn_do_brk_flags(tsk, oldbrk, newbrk-oldbrk, 0) < 0)
		goto out;	//error case

set_brk:
	if (brk <= mm->brk) {
		// TODO: shrink brk should also free memory and cache entry

		// should we multicase munmap while holding the lock?
		tmp_req.tgid = tsk->tgid;
		tmp_req.addr = newbrk;
		tmp_req.len = oldbrk - newbrk;
		for (int i = 0; i < MAX_NUMBER_COMPUTE_NODE + 1; ++i) {
			if (i != nid && tsk->tgids[i]) {
				send_remote_munmap_to_cn(&tmp_req, get_remote_thread_socket(i)->sk);
			}
		}

		mm->brk = brk;

		cacheman_run_lock();
		unsigned long tmp_addr;
		for (tmp_addr = newbrk; tmp_addr < oldbrk; tmp_addr += CACHELINE_MIN_SIZE)
		{
        	try_clear_cache_dir_with_boundry(generate_full_addr(tsk->tgid, tmp_addr), oldbrk);
   		}
		cacheman_run_unlock();

    	printf("BRK-Cleared cache for [0x%lx - 0x%lx], len: %lx\n", newbrk, oldbrk, oldbrk - newbrk);
		goto skip;
	} else {
		mm->brk = brk;
	}
	// populate = newbrk > oldbrk && (mm->def_flags & VM_LOCKED) != 0;
	// add cache line for brk, as did for mmap
#ifdef CACHE_DIR_PRE_OPT
	if (!IS_ERR_VALUE(newbrk))
	{
		unsigned long tmp_addr = oldbrk;
		unsigned long len = newbrk - oldbrk;
		int populated = 1;
#ifndef CACHE_DIR_PRE_POP_IDLE
		int state = CACHELINE_SHARED;
#else
		int state = CACHELINE_IDLE;
#endif
#ifdef CACHE_OWNERSHIP_OPT
			state = CACHELINE_MODIFIED;
#endif
		//pr_vma("start brk is: %lx\n", mm->start_brk);
		// unsigned long len = get_pow_of_two_req_size(len);

		int recheck_free_entry_cnt = 0;
		int num_entry_free;
		int num_entry_needed = min(len, TEST_DEBUG_SIZE_LIMIT) / INITIAL_REGION_SIZE
			+ (len - min(len, TEST_DEBUG_SIZE_LIMIT)) / CACHELINE_MAX_SIZE;

recheck_free_entry:
		cacheman_run_lock();	// To prevent confliction with cache manager thread
		// in case we run out of entry for merge
		if ((num_entry_free = get_free_dir_cnt()) < DYN_CACHE_MIN_DIR_FOR_MMAP + num_entry_needed) {
			cacheman_run_unlock();
			if ((++recheck_free_entry_cnt) >= DYN_CACHE_MMAP_MAX_RETRY) {
				printf("No enough entry: free[%d] need[%d]\n", num_entry_free, num_entry_needed);
			} else {
				printf("Low entry: free[%d] need[%d], wait...\n", num_entry_free, num_entry_needed);
				usleep(DYN_CACHE_SLEEP_US_IF_NO_DIR);
				goto recheck_free_entry;
			}
		}

		// prevent duplicate entries on mmap->munmap->mmap when entries are merged at boundry
		tmp_addr = get_right_uncached_addr(tsk->tgid, oldbrk);
		unsigned long end_addr = get_left_uncached_addr(tsk->tgid, oldbrk + min(len, TEST_DEBUG_SIZE_LIMIT), tmp_addr);

		printf("BRK-Cachline Init: tgid: %d, pid: %d, addr: 0x%lx - 0x%lx, uncached_addr: 0x%lx - 0x%lx, vmflag: 0x%lx, flag: 0x%lx, file: %lu\n",
			   (int)tsk->tgid, (int)tsk->pid, oldbrk, oldbrk + len, tmp_addr, end_addr,
			   0L, 0L, 0L);

		int i;
    	unsigned long index, size;
		for (; tmp_addr < end_addr; )
		{
			/*
			//skip cache entries that overlap with the previous brk
			if ((tmp_addr - mm->start_brk) / INITIAL_REGION_SIZE == 
				(oldbrk - mm->start_brk) / INITIAL_REGION_SIZE) {
				continue;
			}
			*/
		    index = INITIAL_REGION_INDEX;
      		size = INITIAL_REGION_SIZE;
      		while ((tmp_addr % size) || (tmp_addr + size > end_addr)) {
        		// index = get_smaller_cache_region_index(index);
			index = REGION_SIZE_4KB;	// always use 4KB
        		size = (1 << (index + REGION_SIZE_BASE));
			}

			if (!create_new_cache_dir(get_full_virtual_address(tsk->tgid, tmp_addr),
									  state, nid, index))
			{
				printf("BRK-Cachline Init Err: tgid: %d, pid: %d, addr: 0x%lx - 0x%lx, vmflag: 0x%lx, flag: 0x%lx, file: %lu\n",
					   (int)tsk->tgid, (int)tsk->pid, tmp_addr, tmp_addr + size,
					   0L, 0L, 0L);
				// It might be just duplicated entry
				// populated = 0;
				// break;
			}
			tmp_addr += size;
		/*
			printf("BRK-Cachline Init Success: tgid: %d, pid: %d, addr: 0x%lx - 0x%lx, vmflag: 0x%lx, flag: 0x%lx, file: %lu\n",
					   (int)tsk->tgid, (int)tsk->pid, tmp_addr, tmp_addr + INITIAL_REGION_SIZE_EXEC_BRK,
					   0L, 0L, 0L);
		*/
		}
		// 2 MB regions for the remaining ranges
		if (populated)	// no error
		{
			for (; tmp_addr <  oldbrk + len; tmp_addr += CACHELINE_MAX_SIZE)
			{
				if (!create_new_cache_dir(get_full_virtual_address(tsk->tgid, tmp_addr),
										  state, nid, REGION_SIZE_2MB))
				{
					printf("BRK-Cachline Init Err: tgid: %d, pid: %d, addr: 0x%lx - 0x%lx, vmflag: 0x%lx, flag: 0x%lx, file: %lu\n",
						(int)tsk->tgid, (int)tsk->pid, tmp_addr, tmp_addr + INITIAL_REGION_SIZE_EXEC_BRK,
						0L, 0L, 0L);
					// It might be just duplicated entry
					// populated = 0;
					// break;
				}
			}
		}
		cacheman_run_unlock();

		// if (populated)
		{
			//addr |= MMAP_CACHE_DIR_POPULATION_FLAG;
			usleep(1000); //1 ms
			// sleep(1); //DEBUG to ensure the time to update cache directory
			barrier();
		}
	}
#endif
skip:
	sem_post(&mm->mmap_sem);
	return brk;
out:
	sem_post(&mm->mmap_sem);
	return -ENOMEM;
}

int mn_push_data(u16 sender_id, u16 tgid, struct fault_data_struct* data_req){
	int ret = -1;
	struct task_struct *tsk = mn_get_task(tgid);
	unsigned long copied = 0;

	// if there is no existing entry, make a dummy one
    if (!tsk)
    {
        ret = -1;
    }else{
		// if (down_write_killable(&tsk->mm->mmap_sem)) {
		// 	ret = -EINTR;
		// }
		sem_wait(&tsk->mm->mmap_sem);
		// copied = mn_push_data_to_vmas(tsk, &data_req->data, data_req->address, data_req->data_size);
		if (IS_ERR_VALUE(copied) || copied < data_req->data_size)
		{
			ret = -1;
		}else{
			ret = 0;
		}
		sem_post(&tsk->mm->mmap_sem);
	}

	return ret;
}
