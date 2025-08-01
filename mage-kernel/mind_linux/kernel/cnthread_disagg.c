#include <disagg/cnmap_disagg.h>
#include <disagg/cnthread_disagg.h>
#include <disagg/kshmem_disagg.h>
#include <disagg/exec_disagg.h>
#include <disagg/fault_disagg.h>
#include <disagg/profile_points_disagg.h>
#include <disagg/network_disagg.h>
#include <disagg/network_fit_disagg.h>
#include <disagg/print_disagg.h>
#include <disagg/range_lock_disagg.h>
#include <disagg/config.h>
#include <disagg/cpu_alloc_disagg.h>

#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/uaccess.h>		/* probe_kernel_address()	*/
#include <linux/mempolicy.h>
#include <linux/mmu_notifier.h>
#include <linux/inet.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/percpu.h>
#include <linux/random.h>
#include <linux/dma-contiguous.h>

#include <asm/tlb.h>
#include <asm/pgtable_types.h>
#include <asm/pgtable.h>
#include <asm/page_types.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>
#include <asm/byteorder.h>

#include <linux/smp.h>

// These profiling points break down the main loop of all cnthreads.
// (ie: cnthread_main_handler)
// DEFINE_PP(CN_loop_total);

// These profiling points are reclaim-specific. They are part of the main loop
// of the worker and main cnthread. 
DEFINE_PP(CN_reclaim);
DEFINE_PP(CN_rdma_pgcount_); // increments once per RDMAed _page_, not _batch_.
// DEFINE_PP(CN_init_and_rangelock);
// DEFINE_PP(CN_unmap_batch);
// DEFINE_PP(CN_push_to_freelist);
// DEFINE_PP(CN_range_unlock);
// DEFINE_PP(CN_rdma);
// DEFINE_PP(CN_cleanup);

// These profile points are also reclaim-specific. Their times are meaningless;
// they exist only to count events.
// DEFINE_PP(CN_reclaim_success_counter);
// DEFINE_PP(CN_reclaim_skip_counter);

// DEFINE_PP(FH_under_lrulock);
// DEFINE_PP(FH_pop_freelist);

// The first page in our DRAM cache. Freelist and LRU list data points to this.
static struct page *dram_cache_firstpage;
static u64 dram_cache_dma_addr;

// y: Locking order (lock in this order):
//  0. "cache" (bad nickname) range_locks (sleeping locks).
//  1. LRU locks
//  2. cnpage_freelist_lock

struct lru_bucket {
    struct list_head list;
    struct hlist_head hlist[CNTHREAD_LRU_TABLE_HLIST_SIZE];
    spinlock_t lock;
};
static struct lru_bucket cnpage_lru_table[CNTHREAD_LRU_TABLE_SIZE];
// We don't want every reclaimer choosing the same LRU buckets.
DEFINE_PER_CPU(int, preferred_lru_bucket);

struct freelist_cache {
    struct list_head list;
    int size;
};
DEFINE_PER_CPU(struct freelist_cache, freelist_cache);

// Note: When clearing cnpages from the LRU list, make sure to manually set
// `cnpage->lru_list.next = cnpage->lru_list.prev = NULL`.
// Our code uses this invariant to check if a page is on the LRU list.
static LIST_HEAD(cnpage_freelist);
static atomic_t cnpage_freelist_counter;
static spinlock_t cnpage_freelist_lock;

static unsigned long _page_free_threshold = (int)((float)(1.0 - CNTHREAD_CACHED_PRESSURE) * (float)CNTHREAD_MAX_CACHE_BLOCK_NUMBER);

// TLB invalidation related structs
DEFINE_PER_CPU_SHARED_ALIGNED(struct patr_cn_state, patr_cn_state);
DEFINE_PER_CPU_SHARED_ALIGNED(struct patr_fh_state, patr_fh_state);

static struct vm_area_struct dummy_vma;

// HACK: Used for quick pre-deadline bug fix (need quick way to find mm of disagg process).
//       Remove this as soon as you can.
extern struct mm_struct *disagg_mm;

// Triggered by the RoCE module once network initialization is complete.
static wait_queue_head_t wq_network_init_done;
static bool network_init_done = false;
// Triggered by main cnthread once mem initialization etc is complete
static wait_queue_head_t wq_cnthread_init_done;
static bool cnthread_init_done = false;

static int cnthread_main_handler(void *data);
static int cnthread_worker_handler(void *data);

static struct lru_bucket *get_lru_bucket(unsigned long addr)
{
    BUG_ON(addr & ~PAGE_MASK);
    return &cnpage_lru_table[hash_64(addr, CNTHREAD_LRU_TABLE_BITS)];
}

static int get_lru_bucket_num(struct lru_bucket *bucket)
{
    return bucket - &cnpage_lru_table[0];
}

#ifdef MIND_VERIFY_PAGE_CHKSUM
static void print_page_checksum(char *prefix, void *pte_data, void *dma_data, unsigned long addr, unsigned long pfn, unsigned long dma_addr, int target, struct cnthread_page *cnpage)
{
	unsigned long checksum = 0, dmachecksum = 0, *itr;
	for (itr = pte_data; (char *)itr != ((char *)pte_data + PAGE_SIZE); ++itr)
		checksum += *itr;
    for (itr = dma_data; (char *)itr != ((char *)dma_data + PAGE_SIZE); ++itr)
		dmachecksum += *itr;
	pr_cache("%s addr[%lx] checksum[%lx] dmachecksum[%lx] pfn[%lx] dmaaddr[%lx] cnpage[%llx] target[%d]\n", prefix, addr, checksum, dmachecksum, pfn, dma_addr, (u64)cnpage, target);
}
#endif

// Functions for each page
static __always_inline void add_cnpage_to_free_list(struct cnthread_page *cnpage)
{
    lockdep_assert_held(&cnpage_freelist_lock);
    if (unlikely(cnpage->free_list.next || cnpage->free_list.prev))
    {
        printk(KERN_ERR "ERROR:: already in the list!! cnpage: 0x%lx\n", (unsigned long)cnpage);
        return;
    }
    atomic_inc(&cnpage_freelist_counter);
    list_add(&cnpage->free_list, &cnpage_freelist);
}

// Does not clear page dirty bit.
static void __clean_cnpage(struct cnthread_page *cnpage)
{
    if (cnpage->pte) {
        pte_t clean_pte = pte_mkclean(*cnpage->pte);
        set_pte_at(cnpage->mm, cnpage->addr, cnpage->pte, clean_pte);
    }

    cnpage->mm = NULL;
    cnpage->vma = NULL;
    cnpage->addr = 0;
    cnpage->pte = NULL;
    cnpage->pte_lock = NULL;
    cnpage->tgid = 0;

    atomic_set(&cnpage->is_used, CNPAGE_IS_UNUSED);
    atomic_set(&cnpage->kpage->_mapcount, 0); // idle: 0 -> mapped: 1
    atomic_set(&cnpage->kpage->_refcount, 2); // idle: 2 -> mapped: 3

    cnpage->lru_bucket = -1;
    WARN_ON_ONCE(cnpage->lru_list.next || cnpage->lru_list.prev);
    WARN_ON_ONCE(cnpage->free_list.next || cnpage->free_list.prev);
    WARN_ON_ONCE(!hlist_unhashed(&cnpage->lru_hlist));
    cnpage->lru_list.next = cnpage->lru_list.prev = NULL;
    cnpage->free_list.next = cnpage->free_list.prev = NULL;
}

void clean_cnpage(struct cnthread_page *cnpage)
{
    if (unlikely(!cnpage || !cnpage->kpage)) {
        printk(KERN_WARNING "Null struct cnthread_page *\n");
        BUG();
    }
    __clean_cnpage(cnpage);
}

// Caller must lock LRU lists.
static bool cnpage_in_lru(struct cnthread_page *cnpage)
{
    return cnpage->lru_list.next && cnpage->lru_list.prev;
}

static void __cnpage_del_from_lru(struct cnthread_page *cnpage)
{
    hash_del(&cnpage->lru_hlist);
    list_del(&cnpage->lru_list);    // remove from the LRU list
    cnpage->lru_list.next = cnpage->lru_list.prev = NULL;
    cnpage->lru_bucket = -1;
}

// This function assumes that the page is present in the LRU list.
// Caller must lock LRU lists.
static void cnpage_del_from_lru(struct cnthread_page *cnpage)
{
    __cnpage_del_from_lru(cnpage);
}

// Caller must lock LRU lists.
static void cnpage_del_from_lru_if_present(struct cnthread_page *cnpage)
{
    if (cnpage_in_lru(cnpage))
        cnpage_del_from_lru(cnpage);
}

void put_cnpage(struct cnthread_page *cnpage)
{
    struct lru_bucket *lru_bucket;

    // this function removes a page from both hash table and lru_list, then put it back to free_list
    if (unlikely(!cnpage || !cnpage->kpage))
    {
        printk(KERN_WARNING "Null struct cnthread_page *\n");
        BUG();
    }

    // Delete the page from the LRU if it's there
    if (cnpage->lru_bucket != -1) { // cnpage is in LRU
        // NOTE: There is a race condition here!
        // Think abt what happens if `cnpage->lru_bucket` is mutated while we call this fn.
        lru_bucket = &cnpage_lru_table[cnpage->lru_bucket];
        spin_lock(&lru_bucket->lock);
        cnpage_del_from_lru(cnpage);
        spin_unlock(&lru_bucket->lock);
    }

    // Put it back to the free list
    // FIXME: do we need to make the page clean (as zeros)?
    spin_lock(&cnpage_freelist_lock);
    clean_cnpage(cnpage);
    add_cnpage_to_free_list(cnpage);
    spin_unlock(&cnpage_freelist_lock);
}


static void put_cnpage_no_lock(struct cnthread_page *cnpage)
{
    // this function assumes that cnthread_lock, hash_list_lock, free_page_put_lock all locked
    if (unlikely(!cnpage || !cnpage->kpage))
    {
        printk(KERN_WARNING "Null struct cnthread_page **\n");
        BUG();
    }
    cnpage_del_from_lru_if_present(cnpage);
    clean_cnpage(cnpage);
    add_cnpage_to_free_list(cnpage);
}

static void init_lru_table(void)
{
    int i;
    for (i = 0; i < CNTHREAD_LRU_TABLE_SIZE; i++) {
        struct lru_bucket *bucket = &cnpage_lru_table[i];
        INIT_LIST_HEAD(&bucket->list);
        hash_init(bucket->hlist);
        spin_lock_init(&bucket->lock);
    }
    for_each_possible_cpu(i) {
       per_cpu(preferred_lru_bucket, i) = get_random_int() % CNTHREAD_LRU_TABLE_SIZE;
    }
}

static void init_freelist(void)
 {
    int i;
    for_each_possible_cpu(i) {
        struct freelist_cache *pcpu;
        get_cpu();
        pcpu = per_cpu_ptr(&freelist_cache, i);
        INIT_LIST_HEAD(&pcpu->list);
        pcpu->size = 0;
        put_cpu();
    }
    spin_lock_init(&cnpage_freelist_lock);
    atomic_set(&cnpage_freelist_counter, 0);
 }

// Set up the local memory (aka DRAM cache).
//
// This function doesn't perform locking, because it runs during cnthread init.
// (nothing else should be accessing the freelists at this time)
static void cnthread_init_cnpages(void)
{
    struct page *cur_page;
    u64 cur_dma_addr;
    int i;

    // Allocate underlying memory using CMA allocator using a convenient wrapper.
    // Misleading name; this fn doesn't perform any DMA operations.
    dram_cache_firstpage = dma_alloc_from_contiguous(NULL,
            CNTHREAD_MAX_CACHE_BLOCK_NUMBER, 0, GFP_KERNEL);
    BUG_ON(!dram_cache_firstpage);
    pr_info("MIND: allocated DRAM cache at pfn 0x%lx\n", page_to_pfn(dram_cache_firstpage));

    dram_cache_dma_addr = mind_rdma_map_dma_fn(dram_cache_firstpage,
            CNTHREAD_MAX_CACHE_BLOCK_NUMBER * PAGE_SIZE);
    BUG_ON(!dram_cache_dma_addr);

    cur_page = dram_cache_firstpage;
    cur_dma_addr = dram_cache_dma_addr;
    for (i = 0; i < CNTHREAD_MAX_CACHE_BLOCK_NUMBER; i++)
    {
        void *laddr;
        struct cnthread_page *cnpage = kzalloc(sizeof(struct cnthread_page), GFP_KERNEL);
        BUG_ON(!cnpage);

        cnpage->kpage = cur_page;
        cnpage->dma_addr = cur_dma_addr;

        // Zero the page contents (maybe not needed?)
        laddr = kmap(cnpage->kpage);
        BUG_ON(!laddr);
        memset(laddr, 0, PAGE_SIZE);
        kunmap(cnpage->kpage);

        clean_cnpage(cnpage);
        add_cnpage_to_free_list(cnpage);

        cur_page++;
        cur_dma_addr += PAGE_SIZE;
    }
}

static void init_cnthreads(void)
{
    struct cnthread_args *cnthread_args[NUM_CNTHREADS] = {0};
    struct task_struct *task;
    int i;

    // XXX(yash): I don't know if this is still needed.
    BUILD_BUG_ON_MSG(CNTHREAD_WORKER_NUMBER >= (DISAGG_NUM_CORES / 2),
            "Not enough cores in computing blade to run these many cnworkers!");
    BUILD_BUG_ON(NUM_CNTHREADS < 1);

    for (i = 0; i < NUM_CNTHREADS; i++)
    {
        cnthread_args[i] = kzalloc(sizeof(*cnthread_args[i]), GFP_KERNEL);
        if (!cnthread_args[i])
             pr_err("Cannot start cnthread handler daemon!\n");
        cnthread_args[i]->worker_id = i;
    }
    smp_wmb();

    // Start cnthreads.
    task = kthread_create((void *)cnthread_main_handler, (void *)cnthread_args[0], "disagg_main");
    BUG_ON(IS_ERR_OR_NULL(task));
    cnthread_args[0]->cpu = disagg_pin_cnthread_to_core(task);
    wake_up_process(task);

    for (i = 1; i < NUM_CNTHREADS; i++) {
        task = kthread_create((void *)cnthread_worker_handler, (void *)cnthread_args[i],
                "disagg_worker_%d", (int)i);
        BUG_ON(IS_ERR_OR_NULL(task));
        cnthread_args[i]->cpu = disagg_pin_cnthread_to_core(task);
        wake_up_process(task);
    }
}

static void init_patr_structs(void)
{
    int cpu;

    for_each_possible_cpu(cpu) {
        struct patr_cn_state *cn = per_cpu_ptr(&patr_cn_state, cpu);
        struct patr_fh_state *fh = per_cpu_ptr(&patr_fh_state, cpu);

        cpumask_clear(&cn->mask);
        init_llist_head(&fh->reqs);
    }
}

void disagg_cn_thread_init(void)
{
    spin_lock_init(&cnpage_freelist_lock);

    init_waitqueue_head(&wq_network_init_done);
    network_init_done = false;
    init_waitqueue_head(&wq_cnthread_init_done);
    cnthread_init_done = false;

    init_cnpage_range_locks();
    init_lru_table();
    init_freelist();
    init_patr_structs();

    prealloc_kernel_shared_mem();
    memset(&dummy_vma, 0, sizeof(dummy_vma));

    init_test_program_thread_cnt();
    init_cnthreads();
}


// Call this after network init is complete.
void mind_notify_network_init_done(void)
{
    network_init_done = true;
    wake_up_all(&wq_network_init_done);
}
EXPORT_SYMBOL(mind_notify_network_init_done);

uint64_t size_index_to_size(uint16_t s_idx)
{
    return ((uint64_t)DYN_MIN_DIR_SIZE) << s_idx;
}

static u16 hash_ftn(u16 tgid, u64 addr)
{
    return ((tgid + addr) >> CACHELINE_MIN_SHIFT) & 0xffff;
}

int is_transient_state(int state)
{
    return state == CACHE_STATE_IS ||
           state == CACHE_STATE_IM ||
           state == CACHE_STATE_SM ||
           state == CACHE_STATE_SI ||
           state == CACHE_STATE_MI ||
           state == CACHE_STATE_II;
}

// The caller must lock the LRU lists.
static __always_inline struct cnthread_page *find_cnpage_in_lru(
        unsigned int tgid, unsigned long addr, struct lru_bucket *bucket)
{
    struct cnthread_page *cnpage = NULL;
    BUG_ON(addr & ~PAGE_MASK);

    hash_for_each_possible(bucket->hlist, cnpage, lru_hlist, hash_ftn(tgid, (u64)addr))
    {
        if (cnpage->tgid == tgid && cnpage->addr == addr)
            return cnpage;
    }
    return NULL;
}

// Pop a cnpage from the free lists.
// Try the per-CPU caches first; if those fail, then fetch from the global freelists.
static struct cnthread_page *pop_cnpage_from_freelist(void)
{
    struct freelist_cache *cache;
    struct cnthread_page *ret = NULL;
    struct list_head *cur;

    // First, check the per-CPU page cache. This disables preemption.
    cache = &get_cpu_var(freelist_cache);

    // Refill our per-CPU cache.
    if (cache->size == 0) {
        spin_lock(&cnpage_freelist_lock);

        if (list_empty(&cnpage_freelist)) {
            spin_unlock(&cnpage_freelist_lock);
            put_cpu_var(freelist_cache);
            return NULL;
        }

        list_for_each(cur, &cnpage_freelist) {
            cache->size++;
            if (cache->size == CNTHREAD_PERCPU_FREELIST_SIZE)
                 break;
            // Suppose we don't have enough freelist pages to fill our cache.
            // If left unchecked, `list_for_each` will loop until `cur` = `&cnpage_freelist`,
            // and so `list_cut_position` will add nothing to our cache.
            // We break when `cur` is the last list entry, to prevent this.
            if (list_is_last(cur, &cnpage_freelist))
                 break;
        }
        list_cut_position(&cache->list, &cnpage_freelist, cur);

        atomic_sub(cache->size, &cnpage_freelist_counter);
        spin_unlock(&cnpage_freelist_lock);
    }

    // Pop an entry from the free-list.
    ret = list_first_entry(&cache->list, struct cnthread_page, free_list);
    list_del(&ret->free_list);
    ret->free_list.next = ret->free_list.prev = NULL;
    cache->size--;

    put_cpu_var(freelist_cache);
    return ret;
}


// Pop a cnpage from the free list and add it to the LRU list.
//
// Caller should lock LRU lists.
//
// Returns null if the freelist is empty.
static struct cnthread_page *_alloc_cnpage_from_freelist(
    unsigned int tgid, unsigned long addr, struct mm_struct *mm,
    struct lru_bucket *lru_bucket)
{
    struct cnthread_page *cnpage;
    BUG_ON(addr & ~PAGE_MASK);

    cnpage = pop_cnpage_from_freelist();
    if (!cnpage)
         return NULL;

    // initialize cnpage and add to the hash and lru list
    cnpage->addr = addr;
    cnpage->tgid = tgid;
    cnpage->mm = mm;
    cnpage->lru_bucket = get_lru_bucket_num(lru_bucket);
    pr_cache("Assigned new page: tgid[%u] addr[0x%lx] mm[0x%lx]\n",
                tgid, addr, (unsigned long)mm);

    // by adding to the hash, now this cnpage is visible from page fault handler
    hash_add(lru_bucket->hlist, &cnpage->lru_hlist, hash_ftn(tgid, (u64)cnpage->addr));
    list_add(&cnpage->lru_list, &lru_bucket->list);
    return cnpage;
}

// This function gets a cnpage at addr, if one exists in the LRU list.
// Otherwise, it pops one from the freelist and puts it in the LRU list.
//
// It sets `was_cnpage_exist` if a new page was allocated from the freelist.
// It sets `cnpage_out` to the cnpage.
//
// It returns 1 when the LRU list is empty (=> fail). 0 otherwise.
static int __get_cnpage(
        unsigned int tgid, unsigned long addr, struct mm_struct *mm,
        int requesting_new_page, struct cnthread_page **cnpage_out, int *was_cnpage_exist_out)
{
    struct lru_bucket *lru_bucket = get_lru_bucket(addr);
    struct cnthread_page *cnpage;
    // PP_STATE(FH_under_lrulock);
    // PP_STATE(FH_pop_freelist);

    // PP_ENTER(FH_under_lrulock);
    spin_lock(&lru_bucket->lock);

    // y: If there's already a cnpage in the LRU list, retrieve it.
    cnpage = find_cnpage_in_lru(tgid, addr, lru_bucket);
    if (cnpage)
    {
        spin_unlock(&lru_bucket->lock);
        // PP_EXIT(FH_under_lrulock);

        *was_cnpage_exist_out = atomic_read(&cnpage->is_used);
        *cnpage_out = cnpage;
        return 0;
    }

    // Due to the retry routine inside fault_disagg.c, the new_page might be set based on outdated value
    // For example, if we get NACK, and the pte IS NOT present because it is invalidated,
    // the new_page can be still 0 because it is set based on the outdated pte value which IS present.
    // So we can print out a message for debugging but move on to get a new page.
    if (unlikely(!requesting_new_page)) {
        pr_info_ratelimited("Err: didn't request a new page, but no existing page found on LRU. "
                "(pte may be be out-dated? tgid[%u] addr[0x%lx] mm[0x%lx])\n",
                            tgid, addr, (unsigned long)mm);
        BUG();
    }

    // PP_ENTER(FH_pop_freelist);
    // Pop a cnpage off the free-list, and move it to the LRU lists. Call it w/ LRU lists locked.
    cnpage = _alloc_cnpage_from_freelist(tgid, addr, mm, lru_bucket);
    // PP_EXIT(FH_pop_freelist);
    spin_unlock(&lru_bucket->lock);
    // PP_EXIT(FH_under_lrulock);

    if (!cnpage)
        return 1;

    *cnpage_out = cnpage;
    *was_cnpage_exist_out = 0;
    return 0;
}

int get_new_cnpage(unsigned int tgid, unsigned long addr, struct mm_struct *mm,
        struct cnthread_page **cnpage_out, int *was_cacheline_exist)
{
    return __get_cnpage(tgid, addr, mm, 1, cnpage_out, was_cacheline_exist);
}

int get_cnpage(unsigned int tgid, unsigned long addr, struct mm_struct *mm,
        struct cnthread_page **cnpage_out, int *was_cacheline_exist)
{
    return __get_cnpage(tgid, addr, mm, 0, cnpage_out, was_cacheline_exist);
}

// currently not compatible with the new range lock
// ===== THIS OPTIMIZATION CURRENTLY DISABLED BY THE SWITCH =====
// This function is called to create local copy of memory only when:
// - this compute node is the first node request memory allocation to the given virtual address
// - this compute node has exclusive access to the memory
// Switch informs whether the node has 'onwership' or not
void cnthread_create_owner_cacheline(unsigned int tgid, unsigned long addr, struct mm_struct *mm)
{
    BUG();
//     struct cnthread_cacheline *cnline;
// retry_get_free_cache:
//     // Check if there is already populated cacheline
//     spin_lock(&cnthread_lock);
//     spin_lock(&hash_list_lock);
//     cnline = find_cacheline_no_lock(tgid, addr);
//     if (cnline)
//     {   // Skip existing cachline
//         spin_unlock(&cnthread_lock);
//         spin_unlock(&hash_list_lock);
//         return; // Under eviction or parallel access to the same cacheline
//     }

//     // Find new cacheline
//     spin_lock(&free_cacheline_get_lock);
//     if (unlikely(list_empty(&cn_free_cacheline_list) || list_empty(&cn_free_page_list)))
//     {
//         // cnthread_out_of_cache();    // this will kill itself
//         pr_info_ratelimited("Out of DRAM software cache!! (free page: %d [%d], regions: %d[%d])\n",
//                             atomic_read(&cn_free_page_counter), list_empty(&cn_free_page_list),
//                             atomic_read(&cn_free_cacheline_counter), list_empty(&cn_free_cacheline_list));
//         spin_unlock(&free_cacheline_get_lock);
//         spin_unlock(&hash_list_lock);
//         spin_unlock(&cnthread_lock);
//         msleep(100);
//         goto retry_get_free_cache;
//     }
//     else
//     {
//         cnline = _cnthread_allocate_cacheline(tgid, addr, mm);  // Release free_cacheline_get_lock
//         if (cnline)
//         {
//             cnline->ownership = 1;  // This is the only place we set this as 1
//         }
//         spin_unlock(&hash_list_lock);
//         spin_unlock(&cnthread_lock);
//     }
}

// Returns 1 if this fn changed the status, 0 otherwise
int set_cnpage_received(struct cnthread_page *cnpage)
{
    if (!cnpage)
         return 0;
    if (atomic_read(&cnpage->is_used) != CNPAGE_IS_USED) {  // It not already used
        atomic_set(&cnpage->is_used, CNPAGE_IS_RECEIVED);
        return 1;
    }
    return 0;
}

int rollback_received_cnpage(struct cnthread_page *cnpage)
{
    if (cnpage)
    {
        if (atomic_read(&cnpage->is_used) == CNPAGE_IS_RECEIVED) { // It was unused
            atomic_set(&cnpage->is_used, CNPAGE_IS_UNUSED);
            return 1;
        }
    }
    return 0;
}

// This function simply update cnreq to have pte (and related page struct reference counters)
inline int cnthread_add_pte_to_list_with_cnpage(
    pte_t * ptep,
    spinlock_t *pte_lock,
    unsigned long address,
    struct vm_area_struct *vma,
    struct cnthread_page *cnpage, int new_page)
{
    BUG_ON(!cnpage);

    if (new_page && (atomic_read(&cnpage->is_used) != CNPAGE_IS_USED))
    {
        pr_cache("New page fetched - tgid: %u, addr: 0x%lx [0x%lx - 0x%lx] used[p:%d] free[p:%d]\n",
                 cnpage->tgid, address, cnpage->addr, cnpage->addr + PAGE_SIZE,
                 atomic_read(&cnpage->is_used), atomic_read(&cn_free_page_counter));
        // Set up mapping counter (cnthread itself hold 1 reference mapping--all the time)
        atomic_set(&cnpage->kpage->_mapcount, 1); // idle: 0 -> mapped: 1
        atomic_set(&cnpage->kpage->_refcount, 3); // idle: 1 -> mapped: 2
    }

    cnpage->vma = vma ? vma : &dummy_vma;
    cnpage->pte = ptep;
    cnpage->pte_lock = pte_lock;
    smp_wmb();
    atomic_set(&cnpage->is_used, CNPAGE_IS_USED);
    return 0;
}
EXPORT_SYMBOL(cnthread_add_pte_to_list_with_cnpage);

static int __always_inline cnthread_is_pressure(void)
{
    return atomic_read(&cnpage_freelist_counter) <= _page_free_threshold;
}

// Combines 2 functions in one loop for efficiency:
// - Generates remote addresses for all victims.
// - Exits if we don't have a new victim.
static void cnthread_reclaim__calculate_raddrs(struct cnthread_reclaim_victim_batch *batch)
{
    int i;

    read_lock_cnmap_table();
    for (i = 0; i < CNTHREAD_RECLAIM_BATCH_SIZE; i++) {
        struct cnthread_reclaim_victim *victim = &batch->victims[i];
        // Don't add the memory server base offset yet. that's done during RDMA send.
        if (victim->need_push_back)
             victim->raddr = __get_cnmapped_addr(victim->laddr);
    }
    read_unlock_cnmap_table();
}

// Write back the page frame's data back to the switch, if needed.
// (eg: evicting shared page => not needed).
static void cnthread_reclaim__send_rdma(
        struct cnthread_reclaim_victim *victims,
        int batch_size, struct mind_rdma_reqs *ongoing_reqs)
{
    void *qp_handle = current->qp_handle;
    int i, ret;

    // All requests form a "BATCH_SIZE" long linked list.
    // Linked list is "reversed"; reqs[0] is the tail.

    ongoing_reqs->num_reqs = 0;
    for (i = 0; i < batch_size; i++) {
        struct cnthread_reclaim_victim *victim = &victims[i];
        struct mind_rdma_req *req = &ongoing_reqs->reqs[ongoing_reqs->num_reqs];

        if (victim->status != CNTHREAD_RECLAIM_VICTIM_OK)
             continue;
        if (!victim->need_push_back)
            continue;


        // This victim needs to be evicted! Send it to an MR.
        // Assign this victim to an RDMA WR.
        ongoing_reqs->num_reqs++;
        req->sge.addr = victim->cnpage->dma_addr; 
        req->rdma_wr.remote_addr = victim->raddr + ongoing_reqs->mem_server_base_raddr;
    }

    if (ongoing_reqs->num_reqs == 0) // no victims => no RDMA needed
         return;

    PP_INCREMENT(CN_rdma_pgcount_);
    ret = mind_rdma_batched_write_fn(qp_handle, ongoing_reqs, 0);
    if (unlikely(ret)) {
        pr_warn("CNTHREAD: reclaim: ERROR: couldn't issue batched write to victims!\n");
        for (i = 0; i < batch_size; i++)
             victims[i].status = CNTHREAD_RECLAIM_VICTIM_ERROR;
    }
}

// Wait for writes to complete.
static void cnthread_reclaim__recv_rdma_acks(
        struct mind_rdma_reqs *ongoing_reqs,
        struct cnthread_reclaim_victim *victims,
        int num_victims)
{
    if (ongoing_reqs->num_reqs == 0) // No RDMA was needed => no ack will arrive.
         return;

    // Only the last request in the batch has IB_SEND_SIGNALED set => wait for only one ACK.
    mind_rdma_poll_cq_fn(current->qp_handle, ongoing_reqs->reqs);
}

// Put our victims back on the free list. They should already be off the LRU lists at this point.
static void cnthread_reclaim__push_to_freelist(
        struct cnthread_reclaim_victim *victims, int num_victims)
{
    struct list_head tmp_list;
    int i, num_reclaimed = 0;
    // PP_STATE(CN_push_to_freelist);
    // PP_ENTER(CN_push_to_freelist);

    INIT_LIST_HEAD(&tmp_list);

    // Clean the pages and add them to a temporary list (minimize time under lock).
    for (i = 0; i < num_victims; i++) {
        struct cnthread_reclaim_victim *victim = &victims[i];
        if (victim->status != CNTHREAD_RECLAIM_VICTIM_OK)
             continue;

        __clean_cnpage(victim->cnpage);
        list_add(&victim->cnpage->free_list, &tmp_list);
        num_reclaimed++;
        victim->status = CNTHREAD_RECLAIM_VICTIM_SUCCESS;
    }

    spin_lock(&cnpage_freelist_lock);
    list_splice(&tmp_list, &cnpage_freelist);
    spin_unlock(&cnpage_freelist_lock);

    // Technically, this should be inside freelist lock for memory barrier reasons.
    // I don't know if atomic_add forces a WRITE_ONCE equivalent...but it works here.
    atomic_add(num_reclaimed, &cnpage_freelist_counter);
    // PP_EXIT(CN_push_to_freelist);
}

static void cnthread_reclaim__unlock_victims(
        struct cnthread_reclaim_victim *victims,
        int num_victims)
{
    int i;

    // Unlock ranges and update reclaim statistics
    for (i = 0; i < num_victims; i++) {
        switch (victims[i].status) {
        case CNTHREAD_RECLAIM_VICTIM_SUCCESS:
            pr_locks("CN:rlu:%lx\n", victims[i].addr);
            cnpage_unlock_range(victims[i].tgid, &victims[i].lock);
            // PP_INCREMENT(CN_reclaim_success_counter);
            break;
        case CNTHREAD_RECLAIM_VICTIM_SKIP:
            // PP_INCREMENT(CN_reclaim_skip_counter);
            break;
        case CNTHREAD_RECLAIM_VICTIM_ERROR:
             pr_err("cnthread: error during victim reclaim!\n");
             BUG();
             break;
        default:
             BUG();
             break;
        }
    }
    // TODO: If we skip a page: When should we re-add it to the LRU list?
    //       What does the original MIND code do?

    // y: is there a bug if this code path isn't taken? we'll remove the page from
    // the LRU list, and do nothing else. cnthread_reclaim__evict_batch re-adds
    // to free lists as needed; but if it's not taken then we have a bug.
}


// Returns the number of victims reclaimed.
// Caller should lock the LRU lists.
static int cnthread_reclaim__drain_lru_bucket(
        struct lru_bucket *lru_bucket,
        struct cnthread_reclaim_victim *victims,
        int num_victims)
{
    struct cnthread_page *cnpage, *tmp;
    int i = 0;

    list_for_each_entry_safe_reverse(cnpage, tmp, &lru_bucket->list, lru_list)
    {
        struct cnthread_reclaim_victim *victim = &victims[i];
        int ret;

        // Try locking victims. If we can't lock, try another victim.
        ret = cnpage_try_lock_range(cnpage->tgid,
                (cnpage->addr & PAGE_MASK), (cnpage->addr & PAGE_MASK) + PAGE_SIZE,
                &victim->lock);
        if (!ret) // lock failed
             continue;

        // Lock success!
        victim->cnpage = cnpage;
        victim->tgid = victim->cnpage->tgid;
        victim->laddr = victim->cnpage->addr;
        victim->status = CNTHREAD_RECLAIM_VICTIM_OK;
        __cnpage_del_from_lru(cnpage);

        i++;
        if (i == num_victims)
             break;
    }
    return i;
}

// Attempt to find a new victim to reclaim.
// Returns the number of victims found.
static void cnthread_reclaim__init_and_rangelock_victims(
        struct cnthread_reclaim_victim_batch *batch)
{
    int victims_reclaimed = 0;
    int cur_lru = this_cpu_read(preferred_lru_bucket);
    struct cnthread_reclaim_victim *victims = batch->victims;
    int const num_victims = CNTHREAD_RECLAIM_BATCH_SIZE;
    // PP_STATE(CN_init_and_rangelock);

    // PP_ENTER(CN_init_and_rangelock);

    memset(victims, 0, sizeof(*victims) * num_victims);

    // Here, we assume the LRU lists will never run dry.
    while (victims_reclaimed < num_victims)
    {
        struct lru_bucket *lru_bucket = &cnpage_lru_table[cur_lru];
        // TODO(yash): Switch to a try_lock here.
        //             And hey, maybe just pull from a random LRU list instead???? Why iterate?
        spin_lock(&lru_bucket->lock);
        victims_reclaimed += cnthread_reclaim__drain_lru_bucket(
                lru_bucket, victims + victims_reclaimed, num_victims - victims_reclaimed);
        spin_unlock(&lru_bucket->lock);

        cur_lru = (cur_lru + 1) % CNTHREAD_LRU_TABLE_SIZE;
    }
    this_cpu_write(preferred_lru_bucket, cur_lru);

    // Assume all victims in the batch have the same mm.
    batch->mm = disagg_mm;

    // PP_EXIT(CN_init_and_rangelock);
}

// Completes the provided batch by moving it through all stages in the pipeline.
//
// Assumes the rest of the pipeline is empty; call this function on the latest pipeline
// stages first.
//
// This function is intended to fix a deadlock where:
//
// 1. cnthread activates under memory pressure.
// 2. cnthread partially finishes a batch (one run of cnthread_reclaim().
// 3. memory pressure ceases.
// 4. Partially complete batch sits there indefinitely. all victim pages are
//    range-locked, so they block the fault handler. since the fault handler can't
//    continue, memory pressure will never build up and re-activate reclaim.
//
// To avoid this, we call this function from the main cnloop when memory pressure stops.
static void __cnthread_reclaim__drain_pipeline(
        struct cnthread_reclaim_victim_batch *batch)
{
    int const batch_size = CNTHREAD_RECLAIM_BATCH_SIZE;
    // pr_yash("draining batch = %px, status=%d\n", batch, batch->status);

    if (batch->status == CNRECLAIM_BATCH_STARTED_UNMAP) {
        cnthread_reclaim__finish_unmap_victims(batch);
        cnthread_reclaim__calculate_raddrs(batch);
        cnthread_reclaim__send_rdma(batch->victims, batch_size, &batch->ongoing_reqs);

        batch->status = CNRECLAIM_BATCH_STARTED_RDMA;
    }

    if (batch->status == CNRECLAIM_BATCH_STARTED_RDMA) {
        cnthread_reclaim__recv_rdma_acks(&batch->ongoing_reqs,
                batch->victims, batch_size);

        cnthread_reclaim__push_to_freelist(batch->victims, batch_size);
        cnthread_reclaim__unlock_victims(batch->victims, batch_size);
        batch->status = CNRECLAIM_BATCH_DONE;
    }
}

static void cnthread_reclaim__drain_pipeline(
        struct cnthread_reclaim_victim_batch *old_batch,
        struct cnthread_reclaim_victim_batch *mid_batch,
        struct cnthread_reclaim_victim_batch *new_batch)
{
    __cnthread_reclaim__drain_pipeline(old_batch);
    __cnthread_reclaim__drain_pipeline(mid_batch);
    __cnthread_reclaim__drain_pipeline(new_batch);
}

static void init_rdma_reqs_struct(struct mind_rdma_reqs *out, int num_reqs)
{
    out->num_reqs = num_reqs;

    out->reqs = kzalloc(sizeof(*out->reqs) * num_reqs, GFP_KERNEL);
    BUG_ON(!out->reqs);
    mind_rdma_initialize_batched_write_fn(out);
}

static void free_rdma_reqs_struct(struct mind_rdma_reqs *out)
{
    kfree(out->reqs);
}

// (yash): This function handles page reclamation.
// It finds pages, evicts their data, then frees the associated page frames.
//
// NOTE: KEEP THIS FUNCTION IN SYNC WITH cnthread_reclaim__complete_old_batch!
//
// Args:
// - victims: a pre-allocated piece of spare memory to put our victim candidates in.
//            Any pre-existing data in the memory region is destroyed.
// - num_victims: tells us our batch size.
static void cnthread_reclaim(
        struct cnthread_reclaim_victim_batch *new_batch,
        struct cnthread_reclaim_victim_batch *mid_batch,
        struct cnthread_reclaim_victim_batch *old_batch)
{
    int const batch_size = CNTHREAD_RECLAIM_BATCH_SIZE;
    PP_STATE(CN_reclaim);
    // PP_STATE(CN_unmap_batch);

    // pr_yash("new_batch = %px, status=%d\n", new_batch, new_batch->status);

    PP_ENTER(CN_reclaim);

    BUG_ON(new_batch->status != CNRECLAIM_BATCH_DONE);
    cnthread_reclaim__init_and_rangelock_victims(new_batch);
    new_batch->status = CNRECLAIM_BATCH_STARTED_UNMAP;
    update_hiwater_rss(new_batch->mm);

    // PP_ENTER(CN_unmap_batch);
    if (mid_batch->status == CNRECLAIM_BATCH_STARTED_UNMAP)
        cnthread_reclaim__finish_unmap_victims(mid_batch);
    cnthread_reclaim__begin_unmap_victims(new_batch);
    // PP_EXIT(CN_unmap_batch);

    if (mid_batch->status == CNRECLAIM_BATCH_STARTED_UNMAP) {
        // PP_STATE(CN_rdma);

        cnthread_reclaim__calculate_raddrs(mid_batch);
        mid_batch->status = CNRECLAIM_BATCH_STARTED_RDMA;

        // Recv old batch ACKS, then issue RDMA for mid batch.
        // PP_ENTER(CN_rdma);
        if (old_batch->status == CNRECLAIM_BATCH_STARTED_RDMA)
            cnthread_reclaim__recv_rdma_acks(&old_batch->ongoing_reqs,
                    old_batch->victims, batch_size);
        cnthread_reclaim__send_rdma(mid_batch->victims, batch_size, &mid_batch->ongoing_reqs);
        // PP_EXIT(CN_rdma);

        // Complete old batch
        if (old_batch->status == CNRECLAIM_BATCH_STARTED_RDMA) {
            // PP_STATE(CN_cleanup);
            // PP_ENTER(CN_cleanup);
            cnthread_reclaim__push_to_freelist(old_batch->victims, batch_size);
            cnthread_reclaim__unlock_victims(old_batch->victims, batch_size);
            old_batch->status = CNRECLAIM_BATCH_DONE;
            // PP_EXIT(CN_cleanup);
        }
    }

    PP_EXIT(CN_reclaim);
}

// Nothing should be accessing the LRU or freelists at this time.
static void cnthread_cleanup_cnpages(void)
{
    int ret;
    BUG_ON(!mind_rdma_unmap_dma_fn);

    mind_rdma_unmap_dma_fn(dram_cache_dma_addr, CNTHREAD_MAX_CACHE_BLOCK_NUMBER * PAGE_SIZE);
    dram_cache_dma_addr = 0;

    ret = dma_release_from_contiguous(NULL, dram_cache_firstpage, CNTHREAD_MAX_CACHE_BLOCK_NUMBER);
    BUG_ON(!ret);
    dram_cache_firstpage = NULL;
}

// Currently, it just delete from the list
// since we assume the actual page was unmapped by using vm_mummap
// (so should NOT be used *inside* invalidation routine)
int _put_one_cnpage(u16 tgid, unsigned long address, int already_locked)
{
    struct lru_bucket *lru_bucket;
    struct cnthread_page *cnpage;
    struct range_lock page_pos_lock;
    address &= PAGE_MASK;
    range_lock_init(&page_pos_lock, address >> PAGE_SHIFT, address >> PAGE_SHIFT);
    lru_bucket = get_lru_bucket(address);

    if (!already_locked)
        spin_lock(&lru_bucket->lock);

    cnpage = find_cnpage_in_lru(tgid, address, lru_bucket);

    if (cnpage && (atomic_read(&cnpage->is_used) == CNPAGE_IS_USED))
    {
        spin_lock(&cnpage_freelist_lock);
        put_cnpage_no_lock(cnpage);
        spin_unlock(&cnpage_freelist_lock);
    } else {
        if (!already_locked)
        {
            spin_unlock(&lru_bucket->lock);
        }
        return -1;
    }
    if (!already_locked) {
        spin_unlock(&lru_bucket->lock);
    }
    return 0;
}

int put_one_cnpage(u16 tgid, unsigned long address)
{
    return _put_one_cnpage(tgid, address, 0);
}

int put_one_cnpage_no_lock(u16 tgid, unsigned long address)
{
    return _put_one_cnpage(tgid, address, 1);
}

static void __put_all_cnpages(u16 tgid, struct lru_bucket *lru_bucket)
{
    struct cnthread_page *cnpage;
    struct list_head *next = NULL;
    int deleted = 0;

    if (!list_empty(&lru_bucket->list))
    {
        cnpage = container_of(lru_bucket->list.next, struct cnthread_page, lru_list);
        while(1)
        {
            next = cnpage->lru_list.next;
            if (cnpage && cnpage->tgid == tgid)
            {
                if (!put_one_cnpage_no_lock(tgid, cnpage->addr)) // cnthread_lock and hash_list_lock was already hold
                     deleted++;
            }

            if (!next || next == &lru_bucket->list)
                 break;

            cnpage = container_of(next, struct cnthread_page, lru_list);
        }
    }

    // WARNING: Potential infinite recursion here?
    if (deleted > 0)
        __put_all_cnpages(tgid, lru_bucket);  // Check again (to remove pending ones)
}

// this function removes all cnpage with tgid
int put_all_cnpages(u16 tgid)
{
    int i;
    for (i = 0; i < CNTHREAD_LRU_TABLE_SIZE; i++)
        spin_lock(&cnpage_lru_table[i].lock);

    for (i = 0; i < CNTHREAD_LRU_TABLE_SIZE; i++)
         __put_all_cnpages(tgid, &cnpage_lru_table[i]);

    for (i = 0; i < CNTHREAD_LRU_TABLE_SIZE; i++)
        spin_unlock(&cnpage_lru_table[i].lock);
    return 0;
}

static void __cnthread_clean_up_non_existing_entry(u16 tgid, struct mm_struct *mm,
        struct lru_bucket *lru_bucket)
{
    // remove all pages with same tgid
    struct cnthread_page *cnpage;
    struct list_head *next = NULL;
    struct vm_area_struct *vma;

    if (!list_empty(&lru_bucket->list))
    {
        cnpage = container_of(lru_bucket->list.next, struct cnthread_page, lru_list);
        while (1)
        {
            next = cnpage->lru_list.next;
            if (cnpage && cnpage->tgid == tgid)
            {
                vma = find_vma(mm, cnpage->addr); // current or next vma
                if (!vma || !(vma->vm_start > cnpage->addr))    // no vma
                    put_one_cnpage_no_lock(tgid, cnpage->addr); // cnthread_lock and hash_list_lock was already hold

                // Update mm (it was EXECed!)
                if (cnpage->mm != mm)
                    cnpage->mm = mm;
            }
            if (!next || next == &lru_bucket->list)
                break;

            cnpage = container_of(next, struct cnthread_page, lru_list);
        }
    }
}

int cnthread_clean_up_non_existing_entry(u16 tgid, struct mm_struct *mm)
{
    int i;
    for (i = 0; i < CNTHREAD_LRU_TABLE_SIZE; i++)
        spin_lock(&cnpage_lru_table[i].lock);

    for (i = 0; i < CNTHREAD_LRU_TABLE_SIZE; i++)
        __cnthread_clean_up_non_existing_entry(tgid, mm, &cnpage_lru_table[i]);

    for (i = 0; i < CNTHREAD_LRU_TABLE_SIZE; i++)
        spin_unlock(&cnpage_lru_table[i].lock);
    return 0;
}

static void init_cnthread_reclaim_victim_batch(
        struct cnthread_reclaim_victim_batch *batch)
{

    init_rdma_reqs_struct(&batch->ongoing_reqs, CNTHREAD_RECLAIM_BATCH_SIZE);
}

static void free_cnthread_reclaim_victim_batch(
        struct cnthread_reclaim_victim_batch *batch)
{
    free_rdma_reqs_struct(&batch->ongoing_reqs);
}

// A -> B, B -> C, C -> A logic
static void __advance_pipeline(
        struct cnthread_reclaim_victim_batch **a,
        struct cnthread_reclaim_victim_batch **b,
        struct cnthread_reclaim_victim_batch **c)
{
            struct cnthread_reclaim_victim_batch *tmp;

            // swap(a, c)
            tmp = *a;
            *a = *c;
            *c = tmp;

            // swap(b, c)
            tmp = *b;
            *b = *c;
            *c = tmp;
}

// == RECLAIMER ROUTINES == //

// The caller should not hold `mmap_sem`.
static void cnthread_reclaim_many(
        struct cnthread_reclaim_victim_batch *old_batch,
        struct cnthread_reclaim_victim_batch *mid_batch,
        struct cnthread_reclaim_victim_batch *new_batch)
{
    int i;

    if (!disagg_mm) { // HACK: remove as soon as possible.
        msleep_interruptible(25);
        return;
    }

    // Take mmap sem to protect us against concurrent unmaps. ("reclaim" temporarily removes pages
    // from the LRU...so we might be holding on to victims that should be unmapped).
    // But don't hold the lock for too long, otherwise `mmap()` and `munmap()` will fail!
    down_read(&disagg_mm->mmap_sem);

    // TODO grab mmap_sem readable

    // Fill up the cnthread reclaim pipeline for many reclaim iterations.
    for (i = 0; i < CNTHREAD_MMAP_SEM_HOLD_ITERATIONS; i++) {

        if (!cnthread_is_pressure()) {
            cnthread_reclaim__drain_pipeline(old_batch, mid_batch, new_batch);
            up_read(&disagg_mm->mmap_sem);

            msleep_interruptible(25);
            return;
        }

        __advance_pipeline(&new_batch, &mid_batch, &old_batch);
        cnthread_reclaim(new_batch, mid_batch, old_batch);
    }

    // Empty the pipeline and release mmap_sem (this allows mmaps to make progress).
    cnthread_reclaim__drain_pipeline(old_batch, mid_batch, new_batch);
    up_read(&disagg_mm->mmap_sem);
}

static int cnthread_worker_handler(void *data)
{
    struct cnthread_args *args = (struct cnthread_args *)data;
    struct cnthread_reclaim_victim_batch (*victims)[3];
    struct cnthread_reclaim_victim_batch *old_batch, *mid_batch, *new_batch;
    int i;

    allow_signal(SIGKILL | SIGSTOP);

    // Wait until main cnthread has initialized memory regions, etc. 
    while (!cnthread_init_done) {
        wait_event_timeout(wq_cnthread_init_done, cnthread_init_done, 5 * HZ);
        cond_resched();
    }

    pr_info("CN_worker_thread [%d] has been started\n", args->worker_id);

    victims = kzalloc(sizeof(*victims), GFP_KERNEL);
    BUG_ON(!victims);
    for (i = 0; i < 3; i++)
         init_cnthread_reclaim_victim_batch(&(*victims)[i]);
    old_batch = &(*victims)[0];
    mid_batch = &(*victims)[1];
    new_batch = &(*victims)[2];

    // Every RDMA thread needs a dedicate QP+CQ to send requests.
    // We store this QP in current->qp_handle.
    BUG_ON(mind_rdma_get_cnqp_handle_fn == NULL);
    BUG_ON(current->qp_handle != NULL);
    current->qp_handle = mind_rdma_get_cnqp_handle_fn();
    BUG_ON(!current->qp_handle);

    while (1)
    {
        if (kthread_should_stop())
             goto release;
        if (signal_pending(current)) {
            __set_current_state(TASK_RUNNING);
            goto release;
        }

        cnthread_reclaim_many(old_batch, mid_batch, new_batch);
        cond_resched();
    }

release: 
    BUG_ON(!mind_rdma_put_qp_handle_fn);
    mind_rdma_put_qp_handle_fn(current->qp_handle);
    for (i = 0; i < 3; i++)
         free_cnthread_reclaim_victim_batch(&(*victims)[i]);
    kfree(victims);
    kfree(args);
    return 0;
}

static unsigned long cnthread_timer_start = 0;
static int cnthread_main_handler(void *data)
{
    struct cnthread_reclaim_victim_batch (*victims)[3];
    struct cnthread_reclaim_victim_batch *old_batch, *mid_batch, *new_batch;
    unsigned long cnthread_timer_end;
    int i;

    allow_signal(SIGKILL | SIGSTOP);
    pr_info("cnmain: entering cnmain\n");

    // GLOBAL SETUP

    // Wait until roce module is initialized (and connected to the switch)
    pr_info("cnmain: waiting for roce init to finish\n");
    while (!network_init_done) {
        wait_event_timeout(wq_network_init_done, network_init_done, 5 * HZ);
        cond_resched();
    }

    // Initialize DRAM cache pages for all cnthreads (and corresponding DMA mappings)
    pr_info("cnmain: done waiting for roce, starting kmap\n");
    cnthread_init_cnpages();
    pr_info("cnmain: Kmapping and DMA addresses are initialized\n");

    // initialize kernel shared memory for all cnthreads, as we initialized cache invalidation
    // metadata
    init_kernel_shared_mem();

    // LOCAL THREAD SETUP

    // Every RDMA thread needs a dedicate QP+CQ to send requests.
    // We store this QP in current->qp_handle.
    BUG_ON(mind_rdma_get_cnqp_handle_fn == NULL);
    current->qp_handle = mind_rdma_get_cnqp_handle_fn();
    if (current->qp_handle == NULL)
         pr_err("cnmain: ERROR: couldn't assign CNQP handle\n");
    else
         pr_reclaim("cnmain: assigned QP handle 0x%px\n", current->qp_handle);

    victims = kzalloc(sizeof(*victims), GFP_KERNEL);
    BUG_ON(!victims);
    for (i = 0; i < 3; i++)
         init_cnthread_reclaim_victim_batch(&(*victims)[i]);
    old_batch = &(*victims)[0];
    mid_batch = &(*victims)[1];
    new_batch = &(*victims)[2];

    cnthread_init_done = true;
    wake_up_all(&wq_cnthread_init_done);

    pr_info("cnmain: entering loop\n");

    cnthread_timer_start = jiffies;    // intial timer
    while (1)
    {
        if (kthread_should_stop())
            goto release;

        if (signal_pending(current)) {
            __set_current_state(TASK_RUNNING);
            goto release;
        }

        cnthread_reclaim_many(old_batch, mid_batch, new_batch);

        // Print Heartbeat
        cnthread_timer_end = jiffies;
        if (unlikely((cnthread_timer_end > cnthread_timer_start) 
                     && (jiffies_to_msecs(cnthread_timer_end - cnthread_timer_start) >
                         (unsigned long)CNTHREAD_HEARTBEAT_IN_MS)))
        {
            cnthread_timer_start = cnthread_timer_end;
            printk(KERN_DEFAULT "CNTHREAD: HeartBeat (cpu%d): "
                    "Free(p: %d), Reclaimed(p: %ld)\n",
                   (int) smp_processor_id(),
                   atomic_read(&cnpage_freelist_counter),
                   PP_READ_PER_CPU(CN_reclaim, nr));
        }
        cond_resched();
    }

release: // Please release memory here
    // We are shutting down the system...
    BUG_ON(mind_rdma_put_qp_handle_fn == NULL);
    mind_rdma_put_qp_handle_fn(current->qp_handle);
    for (i = 0; i < 3; i++)
         free_cnthread_reclaim_victim_batch(&(*victims)[i]);
    kfree(victims);
    if (data)
        kfree(data);

    cnthread_cleanup_cnpages();
    return 0;
}

/* vim: set tw=99 */
