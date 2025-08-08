#ifndef __CNTHREAD_DISAGGREGATION_H__
#define __CNTHREAD_DISAGGREGATION_H__

#ifndef NOT_IMPORT_LINUX
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/in.h>

#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/sched/signal.h>
#include <disagg/network_disagg.h>
#include <linux/range_lock.h>
#include <disagg/range_lock_disagg.h>

#include <asm/signal.h>
#include <disagg/cache_config.h>
#endif // NOT_IMPORT_LINUX

// Manually Set Parameters

// y: keep this in sync with `network_fit_disagg.h`!
#define DISAGG_NUM_CORES 54
#define DISAGG_TCP_HANDLER_CPU 0
#define DISAGG_FIRST_ASSIGNABLE_CPU 2
#define DISAGG_LAST_ASSIGNABLE_CPU (DISAGG_NUM_CORES - 1)

#define DISAGG_FIRST_ASSIGNABLE_FH_CPU DISAGG_FIRST_ASSIGNABLE_CPU
#define DISAGG_LAST_ASSIGNABLE_FH_CPU  DISAGG_LAST_ASSIGNABLE_CPU
#define DISAGG_FIRST_ASSIGNABLE_CN_CPU DISAGG_FIRST_ASSIGNABLE_CPU
#define DISAGG_LAST_ASSIGNABLE_CN_CPU  DISAGG_LAST_ASSIGNABLE_CPU

// Auto-updated
#define NUM_ASSIGNABLE_CPUS (DISAGG_LAST_ASSIGNABLE_CPU - DISAGG_FIRST_ASSIGNABLE_CPU + 1)

// Manually assigned parameters
#define NUM_CNTHREADS 4
// 1 core for TCP...
#define NUM_FHTHREADS (NUM_ASSIGNABLE_CPUS - NUM_CNTHREADS)

// 1 cnmain, (n-1) cnworkers
#define CNTHREAD_RECLAIM_BATCH_SIZE 256
#define CNTHREAD_WORKER_NUMBER (NUM_CNTHREADS - 1)

// == cache capacity setup == //
// in number of pages: 32768 pages = 128 MB, 131072 = 512 MB
// (131072UL)    // 0.5 GB
// (262144UL)    // 1 GiB
// (524288UL)    // 2 GB
#define CNTHREAD_MAX_CACHE_BLOCK_NUMBER (262144UL)

#define CNTHREAD_CACHELINE_MASK PAGE_MASK
// "0.9" := evictor threads will activate when local memory is 90% occupied. 
// Please keep it as low as possible...reduces evictor threads "overshooting". 
// TODO(yash): try setting this to 0.5 after the current AE replications are due. 
#define CNTHREAD_CACHED_PRESSURE        0.9
#define CNTHREAD_HEARTBEAT_IN_MS 1000
// Reclaim operates while holding mmap_sem.
// Periodically, it shoud drop mmap_sem (so `munmap`, `mmap` can make forward progress).
// This variable defines the number of reclaim iterations before each cnthread drains its pipeline
// and drops mmap_sem.
// - High value => reclaim pipeline stays full...but mmap stays queued for a long time.
// - Low value => faster mmap, but lower reclaim tput.
#define CNTHREAD_MMAP_SEM_HOLD_ITERATIONS 64

#define CNTHREAD_LRU_TABLE_BITS 7
#define CNTHREAD_LRU_TABLE_SIZE (1 << CNTHREAD_LRU_TABLE_BITS)
// TODO: tweak this number, we can definitely make it much smaller.
#define CNTHREAD_LRU_TABLE_HLIST_SIZE (262144UL / 2)
#define CNTHREAD_PERCPU_FREELIST_SIZE 128

#define MIND_FH_POLL_BACKOFF_TIME_NS 100UL

#ifndef NOT_IMPORT_LINUX

struct cnthread_args
{
    int                  cpu;
    int                  worker_id;     // 0 for main, 1~ for workers
};

// NOT USED NOW, left here just for compilation
// Cache state for local version of cache coherence protocol
#define CACHE_STATE_IS 0
#define CACHE_STATE_IM 1
#define CACHE_STATE_SM 2
#define CACHE_STATE_SI 3
#define CACHE_STATE_MI 4
#define CACHE_STATE_II 5
#define CACHE_STATE_S 6
#define CACHE_STATE_M 7
#define CACHE_STATE_I 8
#define CACHE_STATE_MD 9
#define CACHE_STATE_SUCCESS 0xf
#define CACHE_STATE_FAIL    0x0
#define CACHE_STATE_INV_ACK 0xd

// y: Used for setting `struct cnpage->is_used`
enum
{
    CNPAGE_IS_UNUSED = 0,     // not used
    CNPAGE_IS_USED = 1,       // currently used
    CNPAGE_IS_RECEIVED = 2,   // A temporary state. When the fault handler
                              // allocates a formerly-unused page, we put it in
                              // the received state. If we need to rollback,
                              // _then_ we use the fact it's in "received" to
                              // conclude that it was formerly unused => we
                              // need to set it back to unused.
};

struct cnthread_page
{
    struct mm_struct        *mm;
    struct vm_area_struct   *vma;
    struct page             *kpage;
    unsigned int            tgid;
    unsigned long           addr;
    unsigned long           dma_addr;

    // Cached to skip page table walk during reclaim.
    pte_t                   *pte; 
    // Cached to skip page table walk during reclaim.
    spinlock_t              *pte_lock;

    // y: acts as a "status" field for this cnpage.
    atomic_t                is_used;

    // A cnpage is on either the free or LRU lists. Both list_head fields are
    // here just so we can distinguish _which_ list easily.
    //
    // Note: When clearing cnpages from the LRU list, make sure to manually set
    // `cnpage->lru_list.next = cnpage->lru_list.prev = NULL`.
    // Our code uses this invariant to check if a page is on the LRU list.
    struct list_head        free_list;
    struct list_head        lru_list;
    int                     lru_bucket;
    // y: Adds this cnpage to the LRU list hash table.
    struct hlist_node       lru_hlist;
};

// y: these may not all be used.
enum cnthread_reclaim_victim_status {
    CNTHREAD_RECLAIM_VICTIM_OK,      // initial state
    CNTHREAD_RECLAIM_VICTIM_SKIP,    // skip this victim (see status_code)
    CNTHREAD_RECLAIM_VICTIM_ERROR,   // irrecoverable err.
    CNTHREAD_RECLAIM_VICTIM_SUCCESS, // success!
};

enum cnthread_reclaim_victim_status_code {
    CNTHREAD_RECLAIM_VICTIM_SKIP_EMPTY_LRU,
    CNTHREAD_RECLAIM_VICTIM_SKIP_ALREADY_FREED,
    CNTHREAD_RECLAIM_VICTIM_SKIP_UNUSED,
    CNTHREAD_RECLAIM_VICTIM_SKIP_NONANON,
};

struct cnthread_reclaim_victim
{
    struct cnthread_page *cnpage;
    // Address we're faulting on. Presumably this is virtual.
    unsigned long laddr;
    // Address we're RDMAing to.
    u64 raddr;
    // Faulting thread. We use this when locking objects.
    unsigned int tgid;
    // Locks page indices in process address space, so we don't evict twice.
    struct cnpage_lock lock;
    // do we need to evict this data to the memory node?
    bool need_push_back;
    // Stores eviction status, changed as we pass around.
    enum cnthread_reclaim_victim_status status;
    // Stores more detailed information about the status (eg: skip _reason_).
    enum cnthread_reclaim_victim_status_code status_code;
};

enum cnreclaim_victim_batch_status {
    CNRECLAIM_BATCH_DONE,       // empty batch. represents done state.
    CNRECLAIM_BATCH_STARTED_UNMAP,    // batch has sent a TLB IPI (no ack yet)
    CNRECLAIM_BATCH_STARTED_RDMA,   // batch has sent RDMA (no ack yet)
};

struct cnthread_reclaim_victim_batch {
    struct mm_struct *mm;
    enum cnreclaim_victim_batch_status status;
    struct cnthread_reclaim_victim victims[CNTHREAD_RECLAIM_BATCH_SIZE];
    struct patr_req patr_req_pool[NUM_ASSIGNABLE_CPUS];
    struct patr_flush_info patr_flush_info;
    struct mind_rdma_reqs ongoing_reqs;
    struct cpumask tmp_mask; // saves us a dynamic alloc during reclaim.
    union {
        unsigned long patr_start_time;
    };
};

// Let the kernel know that RoCE module is done with network init.
void mind_notify_network_init_done(void);

// Functions to get / release preallocated pages
int get_new_cnpage(unsigned int tgid, unsigned long addr, struct mm_struct *mm,
        struct cnthread_page **cnpage_out, int *was_cacheline_exist);
int get_cnpage(unsigned int tgid, unsigned long addr, struct mm_struct *mm,
        struct cnthread_page **cnpage_out, int *was_cacheline_exist);
void put_cnpage(struct cnthread_page *cnpage);
void cnthread_create_owner_cacheline(unsigned int tgid, unsigned long addr, struct mm_struct *mm);
int set_cnpage_received(struct cnthread_page *cnpage);
int rollback_received_cnpage(struct cnthread_page *cnpage);
// located in `memory.c`
extern void cnthread_reclaim__begin_unmap_victims(struct cnthread_reclaim_victim_batch *batch);
extern void cnthread_reclaim__finish_unmap_victims(struct cnthread_reclaim_victim_batch *batch);

int put_one_cnpage(u16 tgid, unsigned long address);
int put_one_cnpage_no_lock(u16 tgid, unsigned long address);
int put_all_cnpages(u16 tgid);

// y: todo: give this a better name.
int cnthread_add_pte_to_list_with_cnpage(
    pte_t * pte, spinlock_t *pte_lock, unsigned long address, struct vm_area_struct *vma,
    struct cnthread_page *cnpage, int new_page);
int cnthread_clean_up_non_existing_entry(u16 tgid, struct mm_struct *mm);

// Located in TLB and memory code.
extern void disagg_flush_tlb_start(struct cnthread_reclaim_victim_batch *batch);
extern void disagg_flush_tlb_end(struct cnthread_reclaim_victim_batch *batch);
extern void patr_poll_fh_queue(void);

#endif  /* NOT_IMPORT_LINUX */
#endif  /* __CNTHREAD_DISAGGREGATION_H__ */
