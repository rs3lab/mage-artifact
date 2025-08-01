#include <infiniband/verbs.h>
#include <ddc/eviction.h>

#include <ddc/options.hh>
#include <ddc/remote.hh>
#include <osv/mempool.hh>
#include <osv/power.hh>
#include <ddc/stat.hh>
#include <osv/migration-lock.hh>
#include <osv/preempt-lock.hh>
#include <lockfree/queue-mpsc.hh>

#include "cleaner.hh"
#include "init.hh"
#include <ddc/page.hh>
#include "pte.hh"

// #define NO_SG

TRACEPOINT(trace_ddc_evict_request_memory, "size=%lx, hard=%d", size_t, bool);
TRACEPOINT(trace_ddc_evict_request_memory_ret, "size=%lx, hard=%d", size_t,
           bool);
TRACEPOINT(trace_ddc_evict_get_mask, "mask=%lx", uintptr_t);
TRACEPOINT(trace_ddc_evict_get_mask_retrun, "mask=%lx", uintptr_t);
TRACEPOINT(trace_ddc_evict_max_zeros, "zeros=%d %d", int, int);
TRACEPOINT(trace_ddc_evict_max_starts, "starts=%d %d", int, int);
TRACEPOINT(trace_ddc_evict_mask, "mask=%lx", uintptr_t);
TRACEPOINT(trace_ddc_evict_vec, "va=%lx vec=%lx", uintptr_t, uintptr_t);
TRACEPOINT(trace_ddc_request_memory, "free=%lu, s=%lu", size_t, size_t);

// DEBUG Pipelined
TRACEPOINT(trace_ddc_async_pipelined_rdma_poll, "%d %lx", int, uintptr_t);
TRACEPOINT(trace_ddc_async_pipelined_rdma_send, "%d %lx %lx", int, uintptr_t, uintptr_t);
TRACEPOINT(trace_ddc_async_pipelined_reclaim, "%lx", uintptr_t);

STAT_ELAPSED(ddc_dirty_handler);
STAT_ELAPSED(ddc_accessed_handler);
STAT_ELAPSED(ddc_clean_slice);
STAT_ELAPSED(ddc_evict_slice_rest);
STAT_ELAPSED(ddc_reclaim_memory);
STAT_AVG(ddc_clean_slice_scanned);
STAT_AVG(ddc_clean_slice_to_clean);
STAT_AVG(ddc_evict_slice_scanned);
STAT_AVG(ddc_evict_slice_freed);


extern "C" {

uint64_t ddc_eviction_mask_default(uintptr_t vaddr) {
    return 0xFFFFFFFFFFFFFFFF;
}

static ddc_eviction_mask_f mask_f = ddc_eviction_mask_default;

void ddc_register_eviction_mask(ddc_eviction_mask_f f) { mask_f = f; }

static uint64_t ddc_eviction_get_mask(uintptr_t vaddr) {
    trace_ddc_evict_get_mask(vaddr);

    if (vaddr < ddc::vma_middle) return ddc::remote_vec_full;
    uint64_t mask = mask_f(vaddr);
    if (__builtin_popcountll(mask) > 32) return ddc::remote_vec_full;
    trace_ddc_evict_get_mask_retrun(mask);

    // find two big hole
    mask = ~mask;
    int remain = 64;

    int max_zeros[2] = {0, 0};
    int max_starts[2] = {64, 64};

    int count;
    while (remain > 0) {
        count = __builtin_ctzll(mask);
        if (count > remain) count = remain;

        remain -= count;
        mask = ~mask;
        mask >>= count;

        count = __builtin_ctzll(mask);
        if (count > remain) count = remain;

        // count is zero length
        if (max_zeros[0] < count) {
            max_zeros[1] = max_zeros[0];
            max_starts[1] = max_starts[0];

            max_zeros[0] = count;
            max_starts[0] = 64 - remain;
        } else if (max_zeros[1] < count) {
            max_zeros[1] = count;
            max_starts[1] = 64 - remain;
        }

        remain -= count;
        mask = ~mask;
        mask >>= count;
    }
    trace_ddc_evict_max_zeros(max_zeros[0], max_zeros[1]);
    trace_ddc_evict_max_starts(max_starts[0], max_starts[1]);

    if (max_starts[0] > max_starts[1]) {
        uint64_t tmp = max_starts[0];
        max_starts[0] = max_starts[1];
        max_starts[1] = tmp;

        tmp = max_zeros[0];
        max_zeros[0] = max_zeros[1];
        max_zeros[1] = tmp;
    }

    uint8_t starts[3];
    uint8_t counts[3];
    size_t len = 0;

    if (max_starts[0]) {
        starts[len] = 0;
        counts[len] = max_starts[0];
        ++len;
    }
    if (max_starts[0] + max_zeros[0] < 64) {
        starts[len] = max_starts[0] + max_zeros[0];
        counts[len] = max_starts[1] - starts[len];
        ++len;
    }
    if (max_starts[1] + max_zeros[1] < 64) {
        starts[len] = max_starts[1] + max_zeros[1];
        counts[len] = 64 - starts[len];
        ++len;
    }

    uint64_t vec = ddc::pack_vec(starts, counts, len);

    if (!vec) vec = 1;  // todo

    trace_ddc_evict_mask(vec);
    return vec;
}

uint64_t ddc_eviction_mask_test(uintptr_t vaddr) { return mask_f(vaddr); }
}

using TokenArray = std::array<uintptr_t, ddc::pipeline_batch_size>;
using VirtArray = std::array<void*, ddc::pipeline_batch_size>;
using OffsetArray = std::array<uintptr_t, ddc::pipeline_batch_size>;
namespace ddc {

enum {
    eviction_finalize_done = 0,
    eviction_finalize_request = 1,
    eviction_finalize_process = 2,
    // This is used to differentiate between the cancelled request by another 
    // thread running on the same CPU and request processed by the async 
    // reclamation thread
    eviction_finalize_process_done = 3,
};

// For now each CPU will have its own remote_queue
// It is possible that reach remote_queue has multiple qp (for fetch and push)
// but these qp will share the same cq. So poll one remote queue may get cqe from 
// different qps. That is not what we want here.
static std::array<remote_queue, max_cpu> percpu_remote_queues;

static PERCPU(lockfree::linked_item<uint64_t>, entry_finalize);
// RDMA Related percpu buffer
static PERCPU(base_page_slice_t*, _percpu_tlb_batch);
static PERCPU(base_page_slice_t*, _percpu_rdma_batch);
static sched::cpu::notifier _notifier([] () {
    entry_finalize->value = sched::cpu::current()->id;
    *_percpu_tlb_batch = new base_page_slice_t;
    *_percpu_rdma_batch = new base_page_slice_t;
});

TRACEPOINT(trace_ddc_eviction_evict, "page va=%p bytes=%lx va=%p", uintptr_t, uint64_t, void*);
TRACEPOINT(trace_ddc_eviction_tlb_flush, "page va=%p bytes=%lx va=%p", uintptr_t, uint64_t, void*);

constexpr size_t slice_size = 64;

extern std::array<base_page_slice_t, ddc::max_cpu> _per_cpu_active_buffer;
extern std::array<base_page_slice_t, ddc::max_cpu> _per_cpu_clean_buffer;
class evictor : public memory::shrinker, public cleaner {
   public:
    evictor()
        : memory::shrinker("ddc shrinker"),
          cleaner(percpu_remote_queues, 0, current_max_evict) {
    }
    size_t request_memory(size_t s, bool hard) override;

    size_t request_memory_pipelined(size_t s, bool hard);

    size_t _loop_over_clean_slice(size_t cpu_id, 
        base_page_slice_t &slice_clean, base_page_slice_t &slice_active);

    // This is the specialized path which is optimized for response time in sync reclaimation
    // Here we don't try to batch but do it in the sync way
    size_t request_memory_responsive(size_t s, size_t cpu_id);

    void tlb_flush_after(uintptr_t token) override {
        // The page should not be in any list nor buffer
        unsigned cpu_id = sched::cpu::current()->id;
        auto &page = ipt.lookup(static_cast<mmu::phys>(token));
        trace_ddc_eviction_tlb_flush(page.va,
                                 *(uint64_t *)(mmu::phys_to_virt(token)), mmu::phys_to_virt(token));
        SCOPE_LOCK(preempt_lock);
        _per_cpu_active_buffer[cpu_id].push_back(page);
    }
    void push_after(uintptr_t token) override {
        // The page should not be in any list nor buffer
        assert(!sched::thread::current()->migratable());
        unsigned cpu_id = sched::cpu::current()->id;
        auto &page = ipt.lookup(static_cast<mmu::phys>(token));
        trace_ddc_eviction_evict(page.va,
                                 *(uint64_t *)(mmu::phys_to_virt(token)), mmu::phys_to_virt(token));
        SCOPE_LOCK(preempt_lock);
        _per_cpu_clean_buffer[cpu_id].push_back(page);
    }
    state clean_handler(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                        mmu::pt_element<base_page_level> old,
                        uintptr_t offset) override {
        // caller will insert to clean
        return state::OK_FINISH;
    }

    state clean_handler(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                        mmu::pt_element<base_page_level> old, uintptr_t offset,
                        uintptr_t va, uintptr_t &vec) override {
        // caller will insert to clean
        uintptr_t vec_new = get_vec(va);
        if (vec == vec_new) {
            return state::OK_FINISH;
        }
        vec = vec_new;
        return dirty_handler_inner(token, old, offset, vec);
    }

    state clean_handler_up_half(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                        mmu::pt_element<base_page_level> old, uintptr_t offset,
                        uintptr_t va, uintptr_t &vec){
        // caller will insert to clean
        uintptr_t vec_new = get_vec(va);
        if (vec == vec_new) {
            return state::OK_FINISH;
        }
        vec = vec_new;
        return state::OK_PUSHED;
    }


    uint64_t get_vec(uintptr_t va) override {
        return ddc_eviction_get_mask(va);
    }
    size_t evict_slice(ddc::base_page_slice_t& active_buffer, ddc::base_page_slice_t& clean_buffer, 
        bool hard, slice_pages_active_stat_t &active_stats);
    size_t clean_slice(ddc::base_page_slice_t& active_buffer, ddc::base_page_slice_t& clean_buffer, 
        bool do_finalize, bool hard, slice_pages_active_stat_t &active_stats);
    void finalize() {
        unsigned cpu_id = sched::cpu::current()->id;
        finalize(cpu_id);
    }
    void finalize(unsigned cpu_id){
        assert(!sched::thread::current()->migratable());
        while (!tlb_empty(cpu_id) || !cleaner_empty(cpu_id)) {
            tlb_flush(cpu_id);
            poll_all(cpu_id);
        }
    }

    // Used for concurrent version of evictor
    // We need to guarantee that the page is removed from the current list 
    // before being polled by the other and being processed by *_polled 
    // callback
    state process_concurrent(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                  uintptr_t offset, uintptr_t va, mmu::pt_element<base_page_level> old);
    state process_concurrent_mask(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                       uintptr_t offset, uintptr_t va, mmu::pt_element<base_page_level> old, uintptr_t &vec);

    state process_concurrent_mask_no_flush(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                       uintptr_t offset, uintptr_t va, mmu::pt_element<base_page_level> old, uintptr_t &vec);

    memory::pressure pressure_level()
    {
        size_t free_memory = memory::stats::free();
        if ( free_memory < memory::watermark_lo) {
            return memory::pressure::PRESSURE;
        }
        else if (free_memory > memory::watermark_hi ){
            return memory::pressure::RELAXED;
        }
        else {
            return memory::pressure::NORMAL;
        }
    }

private:
    // Different steps for pipelined eviction
    void _update_pte_pipelined(
        base_page_slice_t& slice_active,
        base_page_slice_t& slice_tlb, 
        base_page_slice_t& slice_rdma);

    void _tlb_flush_send_pipelined(
        size_t cpu_id,
        base_page_slice_t& slice_tlb);

    void _tlb_flush_ack_pipelined(
        size_t cpu_id,
        base_page_slice_t& slice_active,
        base_page_slice_t& slice_rdma);

    void _rdma_update_pte_pipelined(
        size_t cpu_id,
        base_page_slice_t& slice_active,
        base_page_slice_t& slice_clean,
        base_page_slice_t& slice_rdma);

    void _rdma_send_pipelined(
        size_t cpu_id,
        base_page_slice_t& slice_rdma);

    void _rdma_ack_pipelined(
        size_t cpu_id,
        base_page_slice_t& slice_clean);

    size_t _reclaim_from_slice_clean_pipelined(
        size_t cpu_id,
        base_page_slice_t& slice_active,
        base_page_slice_t& slice_clean);

    size_t _finalize_pipelined(size_t cpu_id);

    size_t _reclaim_pipelined(size_t cpu_id);
};

evictor::state evictor::process_concurrent(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                  uintptr_t offset, uintptr_t va, mmu::pt_element<base_page_level> old) {
    if (check_skip(old) || !is_ddc(va)){
        // It's okay - fault handler will process this
        return state::OK_SKIP;
    }
    if (old.valid()) {
        if (old.accessed()) {
            return accessed_handler_up_half(token, ptep, old, offset);
        }
        if (old.dirty()) {
            return dirty_handler_up_half(token, ptep, old, offset);
        } else {
            return clean_handler(token, ptep, old, offset);
        }
    }

    abort("noreach\n");
}

evictor::state evictor::process_concurrent_mask(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                       uintptr_t offset, uintptr_t va, mmu::pt_element<base_page_level> old, uintptr_t &vec){
    if (check_skip(old) || !is_ddc(va)) {
        // It's okay - fault handler will process this
        return state::OK_SKIP;
    }
    if (old.valid()) {
        if (old.accessed()) {
            return accessed_handler_up_half(token, ptep, old, offset);
        }
        if (old.dirty()) {
            return dirty_handler_up_half(token, ptep, old, offset, va, vec);
        } else {
            return clean_handler_up_half(token, ptep, old, offset, va, vec);
        }
    }
    abort("noreach\n");
}

// For responsive synchronous reclaimation, skip processing those access bits are set
evictor::state evictor::process_concurrent_mask_no_flush(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                       uintptr_t offset, uintptr_t va, mmu::pt_element<base_page_level> old, uintptr_t &vec){
    if (check_skip(old) || !is_ddc(va)) {
        // It's okay - fault handler will process this
        return state::OK_SKIP;
    }
    if (old.valid()) {
        if (old.accessed()){
            // Skip those whose access bit is set.
            return state::OK_SKIP; 
        }
        if (old.dirty()) {
            return dirty_handler_up_half(token, ptep, old, offset, va, vec);
        } else {
            return clean_handler_up_half(token, ptep, old, offset, va, vec);
        }
    }
    abort("noreach\n");
}

// tracepoints for profiling async reclaim
TRACEPOINT(trace_ddc_async_clean_slice, "%d", size_t);
TRACEPOINT(trace_ddc_async_clean_slice_slice_active, "%d %d", size_t, size_t);
TRACEPOINT(trace_ddc_async_clean_slice_process, "%d %d", size_t, size_t);
TRACEPOINT(trace_ddc_async_clean_slice_ptep, "%d", size_t);
TRACEPOINT(trace_ddc_async_clean_slice_tlb, "%d %d", size_t, size_t);
TRACEPOINT(trace_ddc_async_clean_slice_rdma, "%d %d", size_t, size_t);

// Inside TLB flush
TRACEPOINT(trace_ddc_async_tlb_local_flush, "%d", size_t);
TRACEPOINT(trace_ddc_async_tlb_check_cpus, "%d", size_t);
TRACEPOINT(trace_ddc_async_tlb_send_ipi, "%d", size_t);
TRACEPOINT(trace_ddc_async_tlb_wait_ipi, "%d", size_t);

// Coop TLB
TRACEPOINT(trace_ddc_async_tlb_coop, "%d %d", size_t, size_t);
TRACEPOINT(trace_ddc_async_tlb_nocoop, "%d %d", size_t, size_t);

TRACEPOINT(trace_ddc_clean_slice_update_fail, "");

spinlock_t debug_lock;
PERCPU(u64, ticks_clean_slice);
PERCPU(u64, ticks_clean_slice_slice_active);
PERCPU(u64, ticks_clean_slice_process);
PERCPU(u64, ticks_clean_slice_ptep);
PERCPU(u64, ticks_clean_slice_tlb);
PERCPU(u64, ticks_clean_slice_rdma);
PERCPU(u64, cnt_clean_slice_slice_active);
PERCPU(u64, cnt_clean_slice_process);
PERCPU(u64, cnt_clean_slice_tlb);
PERCPU(u64, cnt_clean_slice_rdma);


size_t evictor::clean_slice(ddc::base_page_slice_t& active_buffer, ddc::base_page_slice_t& clean_buffer, 
    bool do_finalize, bool hard, slice_pages_active_stat &active_stats) {
    u64 tick_process = 0;
    u64 tick_ptep_all = 0;
    u64 tick_rdma = 0;
    u64 tick_tlb = 0;

    auto tick1 = processor::ticks();

    base_page_slice_t slice_active;
    assert(!sched::thread::current()->migratable());
    auto cpu_id = sched::cpu::current() -> id;
    bool back = true; // Unused
    size_t active_list_id = page_list.get_active_list(cpu_id, true, !hard, &back); 
    page_list.slice_pages_active(active_list_id, slice_active, slice_size);
    if (slice_active.size() == 0) {
        return 0;
    }
    auto page = slice_active.begin();
    base_page_slice_t::iterator next;

    auto tick2 = processor::ticks();
    (*cnt_clean_slice_slice_active) ++;
    while (page != slice_active.end()) {
        auto tick3 = processor::ticks();
        if (opt_tlb_flush_mode == "patr")
            flush_tlb_check_and_flush(cpu_id);

        mmu::phys paddr = ipt.lookup(*page);
        uintptr_t offset = va_to_offset(page->va);
        auto tick_ptep = processor::ticks();
        mmu::pt_element<base_page_level> old = page->ptep.read();

#ifdef NO_SG
        auto ret = process_concurrent(paddr, page->ptep, offset, page->va, old);
#else
        uint64_t vec = 0;
        auto ret = process_concurrent_mask(paddr, page->ptep, offset,
                                page->va, old, vec);
        // The problem seems that when the pages are pushed another thread can poll 
        // and then call push_after or tlb_flush_after before the page is erased 
        // from this slice_active
#endif
        active_stats.n_sliced_pages ++;
        assert(slice_active.size() < 600);
        auto tick4 = processor::ticks();
        tick_process += tick4 - tick3;
        tick_ptep_all += tick4 - tick_ptep;
        (*cnt_clean_slice_process) ++;
        switch (ret) {
            case state::OK_SKIP:
                // 다음으로, DONT_NEED는 insert시 삭제
                page++;
                active_stats.n_pages_skip++;
                break;
            case state::OK_FINISH:
                // 이미 clean임
                active_stats.n_pages_finish++;
#ifndef NO_SG
                page->vec = vec;
#endif
                next = slice_active.erase(page);  // push to clean
                WITH_LOCK(preempt_lock){
                    clean_buffer.push_back(*page);
                }
                page = next;
                break;
            case state::OK_TLB_PUSHED:
                active_stats.n_pages_tlb++;
                // active로 // flush 후에 집어 넣음
                page = slice_active.erase(page);  // do not push back
                // We first need to remove the page from the slice
                // Otherwise when we push, other threads may poll
                // and try to push the page to another list
                accessed_handler_bottom_half(paddr);
                tick_tlb += processor::ticks() - tick4;
                (*cnt_clean_slice_tlb)++;
                break;
            case state::OK_PUSHED:
                active_stats.n_pages_evict++;
                // clean으로 // push 후에 집어 넣음
#ifndef NO_SG
                page->vec = vec;
#endif
                page = slice_active.erase(page);  // do not push back

                // We first need to remove the page from the slice
                // Otherwise when we push, other threads may poll
                // and try to push the page to another list
#ifndef NO_SG
                dirty_handler_bottom_half(paddr, old, offset, vec);
#else
                dirty_handler_bottom_half(paddr, old, offset);
#endif
                tick_rdma += processor::ticks() - tick4;
                (*cnt_clean_slice_rdma)++;
                break;
            case state::PTE_UPDATE_FAIL:
                active_stats.n_pages_failed++;
                trace_ddc_clean_slice_update_fail();
                // 재시도
                continue;
            default:
                abort("noreach");
        }
    }
    if (do_finalize) finalize();
    size_t cleaned = clean_buffer.size() << base_page_shift;
    WITH_LOCK(preempt_lock){
        active_buffer.splice(active_buffer.end(), slice_active);
    }
    assert(slice_active.size() < 4096);
    assert(active_buffer.size() < 4096);

    *ticks_clean_slice += processor::ticks() - tick1;
    *ticks_clean_slice_slice_active += tick2 - tick1;
    *ticks_clean_slice_process += tick_process;
    *ticks_clean_slice_ptep += tick_ptep_all;
    *ticks_clean_slice_tlb += tick_tlb;
    *ticks_clean_slice_rdma += tick_rdma;
    return cleaned;
}
STAT_ELAPSED(ddc_evict_slice);
STAT_ELAPSED(ddc_evict_slice_first);
STAT_ELAPSED(ddc_evict_slice_pages_clean);
STAT_ELAPSED(ddc_evict_slice_pages_clean_second);
STAT_ELAPSED(ddc_evict_slice_push_pages_both);
STAT_ELAPSED(ddc_evict_slice_push_pages_active);
TRACEPOINT(trace_ddc_evict_slice_pages_clean, "");
TRACEPOINT(trace_ddc_evict_slice_pages_clean_ret, "");
TRACEPOINT(trace_ddc_evict_push_pages_active, "");
TRACEPOINT(trace_ddc_evict_push_pages_active_ret, "");
TRACEPOINT(trace_ddc_evict_push_pages_both, "");
TRACEPOINT(trace_ddc_evict_push_pages_both_ret, "");
TRACEPOINT(trace_ddc_evict_accessed, "%d", uint64_t);
TRACEPOINT(trace_ddc_async_reclaimed_pages, "%d", uint64_t);
TRACEPOINT(trace_ddc_sync_reclaimed_pages, "%d", uint64_t);
size_t evictor::evict_slice(ddc::base_page_slice_t& active_buffer, ddc::base_page_slice_t& clean_buffer, 
    bool hard, slice_pages_active_stat &active_stats) {
    size_t freed = 0;
    base_page_slice_t slice_clean;
    //slice_pages_clean is thread safe
    unsigned cpu_id = sched::cpu::current()->id;
    size_t clean_list_id = page_list.get_clean_list(cpu_id, true, true);
    page_list.slice_pages_clean(clean_list_id, slice_clean, slice_size);
    if (hard) trace_ddc_evict_slice_pages_clean_ret();
    if (slice_clean.size() == 0) {
        clean_slice(active_buffer, clean_buffer, false, hard, active_stats);
        active_stats.n_slices ++;
        WITH_LOCK(preempt_lock){
            if (clean_buffer.size()< slice_size) {
                page_list.slice_pages_clean(clean_list_id, slice_clean, slice_size - clean_buffer.size());
            }
            slice_clean.splice(slice_clean.end(), clean_buffer);
            if (slice_clean.size() == 0 && active_buffer.size() != 0){
                bool back = true;
                size_t active_list_id = page_list.get_active_list(0, false, !hard, &back);
                page_list.push_pages_active(active_list_id, active_buffer, back);
            }
        }
        if (slice_clean.size() == 0) {
            return freed;
        }
    }

    auto page = slice_clean.begin();
    while (page != slice_clean.end()) {
        if (opt_tlb_flush_mode == "patr")
            flush_tlb_check_and_flush(cpu_id);
        auto pte = page->ptep.read();
        if (pte.empty()) {
            // removed by MADV_DONTNEED
            // Skip.
            // this page will be removed by page_list.push_pages
            page++;
            continue;
        }
        assert(pte.valid());

        if (pte.accessed()) {
            auto new_pte = pte;
            
            // NOTE: I don't understand why I mark dirty here 
            new_pte.set_dirty(true);
            if (!page->ptep.compare_exchange(pte, new_pte)) {
                trace_ddc_clean_slice_update_fail();
            }


            auto next = slice_clean.erase(page);
            active_buffer.push_back(*page);
            page = next;
            trace_ddc_evict_accessed(pte.dirty());
        } else {
            assert(!pte.dirty());
            assert(page->va);
#ifndef NO_SG
            assert(page->vec);
            trace_ddc_evict_vec(page->va, page->vec);
#endif
            // TODO: FIXME! when SG not enabled, no page->vec!
            auto pte_new = remote_pte_vec(page->ptep, page->vec);

            if (!page->ptep.compare_exchange(pte, pte_new)) {
                // retry
                continue;
            }
            auto next = slice_clean.erase(page);
            freed += base_page_size;
            page->st = page_status::UNTRACK;
            page_list.free_not_used_page(*page);
            page = next;
        }
    }
    if (freed) trace_ddc_async_reclaimed_pages(freed / base_page_size);

    WITH_LOCK(preempt_lock){
        bool back = true;
        size_t active_list_id = page_list.get_active_list(0, false, !hard, &back);
        size_t clean_list_id = page_list.get_clean_list(cpu_id, false, true);
        page_list.push_pages_both(active_list_id, active_buffer, back, clean_list_id, slice_clean);
    }
    return freed;
}

// Since this request memory will be called concurrently by multiple
// threads, we have to make sure this is thread-safe 
// For now we make the how request_memory function migration-disabled
TRACEPOINT(trace_ddc_slice_pages_active_number, "%u %u %u %u %u %u %u %u",
uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

TRACEPOINT(trace_ddc_sync_reclaim, "%d %d %d %d", size_t, size_t, size_t, size_t);
TRACEPOINT(trace_ddc_sync_reclaim_rdma, "%d %d", size_t, size_t);
TRACEPOINT(trace_ddc_sync_reclaim_slice_active, "%d", size_t);
TRACEPOINT(trace_ddc_sync_reclaim_push_active, "%d", size_t);
TRACEPOINT(trace_ddc_sync_reclaim_loop_active, "%d", size_t);
TRACEPOINT(trace_ddc_sync_reclaim_loop_active_translate, "%d", size_t);
TRACEPOINT(trace_ddc_sync_reclaim_loop_active_typing, "%d", size_t);
TRACEPOINT(trace_ddc_sync_reclaim_loop_clean, "%d", size_t);

TRACEPOINT(trace_ddc_async_evict_slice, "%d", size_t);
TRACEPOINT(trace_ddc_async_finalize, "%d", size_t);

size_t evictor::request_memory(size_t s, bool hard) {

    // NOTE: This method can be invoked concurrently by app threads and 
    // kernel threads
    u64 ticks_clean_slice0 = *ticks_clean_slice;
    u64 ticks_clean_slice_slice_active0 = *ticks_clean_slice_slice_active;
    u64 ticks_clean_slice_process0 = *ticks_clean_slice_process;
    u64 ticks_clean_slice_ptep0 = *ticks_clean_slice_ptep;
    u64 ticks_clean_slice_tlb0 = *ticks_clean_slice_tlb;
    u64 ticks_clean_slice_rdma0 = *ticks_clean_slice_rdma;
    u64 cnt_clean_slice_slice_active0 = *cnt_clean_slice_slice_active;
    u64 cnt_clean_slice_process0 = *cnt_clean_slice_process;
    u64 cnt_clean_slice_tlb0 = *cnt_clean_slice_tlb;
    u64 cnt_clean_slice_rdma0 = *cnt_clean_slice_rdma;

    // General TLB
    u64 ticks_tlb_local_flush0 = *tlb_local_flush;
    u64 ticks_tlb_check_cpus0 = *tlb_check_cpus;
    u64 ticks_tlb_send_ipi0 = *tlb_send_ipi;
    u64 ticks_tlb_wait_ipi0 = *tlb_wait_ipi;
    // Coop
    u64 ticks_tlb_coop0 = *tick_tlb_coop;
    u64 ticks_tlb_nocoop0 = *tick_tlb_nocoop;
    u64 cnt_tlb_coop0 = *cnt_tlb_coop;
    u64 cnt_tlb_nocoop0 = *cnt_tlb_nocoop;

    auto start_tick = processor::ticks();

    trace_ddc_evict_request_memory(s, hard);
    size_t freed  = 0, newly_freed = 0;
    slice_pages_active_stat_t active_stats = {0};
    bool is_push_back = true;

    // This function can be invoked by sync reclaim from APP threads. 
    // Lock migration 
    SCOPE_LOCK(migration_lock);

    auto cpu_id = sched::cpu::current() -> id;

    // Differentiate sync reclaim and kswapd
    bool is_sync_reclaim = sched::thread::current() -> is_app();
    if (!is_sync_reclaim && opt_async_reclaim == "pipelined"){
        return request_memory_pipelined(s, hard);
    } else if (is_sync_reclaim && opt_sync_reclaim == "response"){
        size_t max_no_flush_trial  = 4;
        for (size_t i = 0; i < max_no_flush_trial; i++){
            newly_freed = request_memory_responsive(s, cpu_id);
            freed += newly_freed;
            if(freed > s) break;
        }
        if (freed >= s){
            // TODO: Clean the percpu RDMA queue. Otherwise some page can stuck for ever

            // Do cleanup 
            auto &active_buffer = _per_cpu_active_buffer[cpu_id];
            auto &clean_buffer = _per_cpu_clean_buffer[cpu_id];
            SCOPE_LOCK(preempt_lock);
            size_t active_list_id = page_list.get_active_list(
                cpu_id, false, !hard, &is_push_back);
            size_t clean_list_id = page_list.get_clean_list(cpu_id, false, true);
            page_list.push_pages_both(
                active_list_id, active_buffer, is_push_back, clean_list_id, clean_buffer);
            assert(active_buffer.empty() && clean_buffer.empty());
            trace_ddc_sync_reclaim(1, freed, s, processor::ticks() - start_tick);

            // TODO: Cleanup percpu RDMA queue!
            return freed;
        } // else fall through to more complicated reclaim
        trace_ddc_sync_reclaim(0, freed, s, processor::ticks() - start_tick);
    }
    
    // TODO: Make the buffer per CPU variable
    auto &active_buffer = _per_cpu_active_buffer[cpu_id];
    auto &clean_buffer = _per_cpu_clean_buffer[cpu_id];

    // NOTE: A quick check of what timer to use
    
    auto tick1 = processor::ticks();
    while (freed < s){
        newly_freed = evict_slice(active_buffer, clean_buffer, hard, active_stats);
        freed += newly_freed;
    }
    auto tick2 = processor::ticks();
    finalize(cpu_id);
    ::memory::clear_reclaim_buf();

    // Prevent kswapd and sync reclaim operate on the same DS
    SCOPE_LOCK(preempt_lock);
    size_t active_list_id = page_list.get_active_list(cpu_id, false, !hard, &is_push_back);
    size_t clean_list_id = page_list.get_clean_list(cpu_id, false, true);
    page_list.push_pages_both(active_list_id, active_buffer, is_push_back, clean_list_id, clean_buffer);
    assert(active_buffer.empty() && clean_buffer.empty());
    auto tick3 = processor::ticks();
    
    trace_ddc_async_evict_slice(tick2 - tick1);
    trace_ddc_async_finalize(tick3 - tick2);
    trace_ddc_async_clean_slice(*ticks_clean_slice - ticks_clean_slice0);
    trace_ddc_async_clean_slice_slice_active(
        *cnt_clean_slice_slice_active - cnt_clean_slice_slice_active0,
        *ticks_clean_slice_slice_active - ticks_clean_slice_slice_active0);
    trace_ddc_async_clean_slice_process(
        *cnt_clean_slice_process - cnt_clean_slice_process0,
        *ticks_clean_slice_process - ticks_clean_slice_process0);
    trace_ddc_async_clean_slice_ptep(
        *ticks_clean_slice_ptep - ticks_clean_slice_ptep0);
    trace_ddc_async_clean_slice_tlb(
        *cnt_clean_slice_tlb - cnt_clean_slice_tlb0,
        *ticks_clean_slice_tlb - ticks_clean_slice_tlb0);
    trace_ddc_async_clean_slice_rdma(
        *cnt_clean_slice_rdma - cnt_clean_slice_rdma0,
        *ticks_clean_slice_rdma - ticks_clean_slice_rdma0);
    
    // TLB Internal
    trace_ddc_async_tlb_local_flush(*tlb_local_flush - ticks_tlb_local_flush0);
    trace_ddc_async_tlb_check_cpus(*tlb_check_cpus - ticks_tlb_check_cpus0);
    trace_ddc_async_tlb_send_ipi(*tlb_send_ipi - ticks_tlb_send_ipi0);
    trace_ddc_async_tlb_wait_ipi(*tlb_wait_ipi - ticks_tlb_wait_ipi0);
    
    // Coop TLB
    trace_ddc_async_tlb_coop(*cnt_tlb_coop - cnt_tlb_coop0, *tick_tlb_coop - ticks_tlb_coop0);
    trace_ddc_async_tlb_nocoop(*cnt_tlb_nocoop - cnt_tlb_nocoop0, *tick_tlb_nocoop - ticks_tlb_nocoop0);

    trace_ddc_evict_request_memory_ret(freed, hard);
    return freed;
}

size_t evictor::_loop_over_clean_slice(size_t cpu_id, 
    base_page_slice_t &slice_clean, base_page_slice_t &slice_active){
    size_t freed = 0;
    // Then go over slice clean
    auto page = slice_clean.begin();
    while (page != slice_clean.end()) {
        if (opt_tlb_flush_mode == "patr")
            flush_tlb_check_and_flush(cpu_id);
        auto pte = page->ptep.read();
        if (pte.empty()) {
            // removed by MADV_DONTNEED
            // Skip.
            // this page will be removed by page_list.push_pages
            page++;
            continue;
        }
        assert(pte.valid());

        if (pte.accessed()) {
            auto new_pte = pte;
            
            // NOTE: I don't understand why I mark dirty here 
            new_pte.set_dirty(true);
            if (!page->ptep.compare_exchange(pte, new_pte)) {
                trace_ddc_clean_slice_update_fail();
            }
            auto next = slice_clean.erase(page);
            slice_active.push_back(*page);
            page = next;
            trace_ddc_evict_accessed(pte.dirty());
        } else {
            assert(!pte.dirty());
            assert(page->va);
#ifndef NO_SG
            assert(page->vec);
            trace_ddc_evict_vec(page->va, page->vec);
#endif
            // TODO: FIXME! No page->vec available
            auto pte_new = remote_pte_vec(page->ptep, page->vec);

            if (!page->ptep.compare_exchange(pte, pte_new)) {
                // retry
                continue;
            }
            auto next = slice_clean.erase(page);
            freed += base_page_size;
            page->st = page_status::UNTRACK;
            page_list.free_not_used_page(*page);
            page = next;
        }
    }
    if (freed) trace_ddc_sync_reclaimed_pages(freed / base_page_size);
    return freed;
}

size_t evictor::request_memory_responsive(size_t s, size_t cpu_id){
    // v0: Let's try no flush and see how often we fall back
    // We put the active pages back to active list and only work on those
    // whose access bit is unset. 
    // TODO: Check scan flush mode
    base_page_slice_t slice_clean;
    base_page_slice_t slice_active;
    bool back = true;
    size_t freed = 0;
    uint64_t start_tick = 0;
    uint64_t end_tick = 0;
    uint64_t total_tick = 0;
    uint64_t total_tick1 = 0;
    uint64_t total_tick2 = 0;

    uint64_t count = 0;
    size_t clean_list_id = page_list.get_clean_list(cpu_id, true, true);
    page_list.slice_pages_clean(clean_list_id, slice_clean, slice_size);
    start_tick = processor::ticks();
    if (slice_clean.size() == 0){
        // Slice some page from active list
        size_t active_list_id = page_list.get_active_list(cpu_id, true, true, &back);
        page_list.slice_pages_active(active_list_id, slice_active, 2 * slice_size);
        end_tick = processor::ticks();
        trace_ddc_sync_reclaim_slice_active(end_tick - start_tick);
        auto end_tick1 = end_tick;
        
        // Return if we cannot slice anything from either clean_list or active_list
        if (slice_active.size() == 0)
            return 0;
        auto page = slice_active.begin();
        base_page_slice_t::iterator next;
        while (page != slice_active.end()){
            if (opt_tlb_flush_mode == "patr")
                flush_tlb_check_and_flush(cpu_id);
            start_tick = processor::ticks();
            
            mmu::phys paddr = ipt.lookup(*page);
            uintptr_t offset = va_to_offset(page->va);
            mmu::pt_element<base_page_level> old = page->ptep.read();
            // TODO: Support no SG
            uint64_t vec = 0;
            auto ret = process_concurrent_mask_no_flush(paddr, page->ptep, offset,
                                    page->va, old, vec);
            end_tick = processor::ticks();
            total_tick1 += end_tick- start_tick;
            
            switch (ret) {
                case state::OK_SKIP:
                    // 다음으로, DONT_NEED는 insert시 삭제
                    page++;
                    break;
                case state::OK_FINISH:
    #ifndef NO_SG
                    page->vec = vec;
    #endif
                    next = slice_active.erase(page);  // push to clean
                    slice_clean.push_back(*page);
                    page = next;
                    break;
                case state::OK_TLB_PUSHED:
                    // Should not reach here
                    abort("noreach");
                case state::OK_PUSHED:
    #ifndef NO_SG
                    page->vec = vec;
    #endif
                    page = slice_active.erase(page);

                    start_tick = processor::ticks();
    #ifndef NO_SG
                    dirty_handler_bottom_half(paddr, old, offset, vec);
    #else
                    dirty_handler_bottom_half(paddr, old, offset);
    #endif
                    total_tick +=  processor::ticks() - start_tick;
                    count += 1;
                    break;
                case state::PTE_UPDATE_FAIL:
                    trace_ddc_clean_slice_update_fail();
                    continue;
                default:
                    abort("noreach");
            }
            total_tick2 += processor::ticks() - end_tick;

        }
        trace_ddc_sync_reclaim_loop_active(processor::ticks() - end_tick1);
        trace_ddc_sync_reclaim_loop_active_translate(total_tick1);
        trace_ddc_sync_reclaim_loop_active_typing(total_tick2);
    }
    if (total_tick){
        trace_ddc_sync_reclaim_rdma(count, total_tick);
    }
    start_tick = processor::ticks();
    // First put slice_active back
    if (slice_active.size()){
        size_t active_list_id = page_list.get_active_list(cpu_id, false, true, &back);
        page_list.push_pages_active(active_list_id, slice_active, back);
    }
    end_tick = processor::ticks();
    trace_ddc_sync_reclaim_push_active(end_tick - start_tick);
    freed += _loop_over_clean_slice(cpu_id, slice_clean, slice_active);
    // We splice those in the _percpu_clean_buffer
    slice_clean.splice(slice_clean.end(), _per_cpu_clean_buffer[cpu_id]);
    freed += _loop_over_clean_slice(cpu_id, slice_clean, slice_active);

    trace_ddc_sync_reclaim_loop_clean(processor::ticks() - end_tick);

    if (slice_clean.size() || slice_active.size()){
        size_t active_list_id = page_list.get_active_list(cpu_id, false, true, &back);
        size_t clean_list_id = page_list.get_clean_list(cpu_id, false, true);
        page_list.push_pages_both(active_list_id, slice_active, back, clean_list_id, slice_clean);
    }

    // TODO: Try to poll the RDMA to cleanup
    // TODO: Try to add flush?

    
    return freed;
}


// The 3-stage pipelined eviction
// Used for RDMA
PERCPU(TokenArray, _percpu_paddrs);
PERCPU(VirtArray, _percpu_virts);
PERCPU(OffsetArray, _percpu_offsets);

void evictor::_update_pte_pipelined(
    base_page_slice_t& slice_active,
    base_page_slice_t& slice_tlb, 
    base_page_slice_t& slice_rdma){
    base_page_slice_t::iterator next;
    auto page = slice_active.begin();
    auto ret = state::OK_SKIP;
    auto cpu_id = sched::cpu::current() -> id;
    while (page != slice_active.end()){
        if (opt_tlb_flush_mode == "patr")
            flush_tlb_check_and_flush(cpu_id);
        // Physical Address
        mmu::phys paddr = ipt.lookup(*page);
        // Remote offset
        uintptr_t offset = va_to_offset(page->va);
        mmu::pt_element<base_page_level> old = page->ptep.read();
        // TODO: NO_SG: partial write back

        // NOTE: Here we still keep the assumption that the pages 
        // whose access bit is unset is not in any CPU's TLB
        if (check_skip(old) || !is_ddc(page->va)){
            ret = state::OK_SKIP;
        } else if (old.valid()){
            // We will flush the entry anyway
            if (old.accessed()) {
                ret = accessed_handler_up_half(paddr, page->ptep, old, offset);
            } else {
                if (opt_lru_mode == "static_fifo"){
                    // Don't care. Just flush anyway
                    ret = state::OK_TLB_PUSHED;
                }
                else if (opt_lru_mode == "static_lru"){
                    // Anyway clear the dirty bit and go to RDMA
                    ret = state::OK_PUSHED;
                } else {
                    abort("Not supported in pipelined");
                }
            }
        }

        // We should only need to handle 3 states
        switch (ret){
            case state::OK_SKIP:
                // Skip 
                page++;
                break;
            case state::OK_TLB_PUSHED:
                // Move to TLB staging buffer
                next = slice_active.erase(page);
                slice_tlb.push_back(*page);
                page = next;
                break;
            case state::OK_PUSHED:
                // Move to RDMA staging buffer
                next = slice_active.erase(page);
                slice_rdma.push_back(*page);
                page = next;
                break;
            case state::PTE_UPDATE_FAIL:
                // Retry with this page
                continue;
            default:
                abort("noreach");
        }
    }
}

// TODO: Check about concurrent flush (another flush before this one is acked)
// Only send TLB by IPI
void evictor::_tlb_flush_send_pipelined(
    size_t cpu_id,
    base_page_slice_t& slice_tlb){
    // Move slice_tlb to tlb_batch and check the emptiness of tlb_batch
    if (slice_tlb.size()){
        if (opt_tlb_flush_mode == "patr"){
            SCOPE_LOCK(preempt_lock);
            mmu::flush_tlb_cooperative();
        }
        else if (opt_tlb_flush_mode == "batch") {
            mmu::flush_tlb_all_send_pipelined(cpu_id);
        }
        else {
            abort("Unknown TLB flush mode");
        }
        assert((*_percpu_tlb_batch)->empty());
        (*_percpu_tlb_batch)->splice(
            (*_percpu_tlb_batch)->end(), slice_tlb);
    }
}

// Ack the prev batch
void evictor::_tlb_flush_ack_pipelined(
    size_t cpu_id,
    base_page_slice_t& slice_active,
    base_page_slice_t& slice_rdma){
    // Move all the pages from tlb_batch to the temp buf
    if ((*_percpu_tlb_batch) -> size()){
        if (opt_tlb_flush_mode == "patr"){
            SCOPE_LOCK(preempt_lock);
            int done = mmu::flush_tlb_cooperative_check();
            if (!done){
                // This fallback will be very costly and 
                // can be only covered if the time waiting 
                // for RDMA is high enough
                mmu::flush_tlb_all();
            }
            
        } else if (opt_tlb_flush_mode == "batch"){
            // Wait until done
            mmu::flush_tlb_all_ack_pipelined(cpu_id);
        } else {
            abort("Unknown TLB flush mode");
        }
        if (opt_lru_mode == "static_fifo"){
            // Prepare to flush
            assert(slice_rdma.size() == 0);
            slice_rdma.splice(slice_rdma.end(), (**_percpu_tlb_batch));
        } else if (opt_lru_mode == "static_lru"){
            // Anyway put these pages back
            slice_active.splice(slice_active.end(), (**_percpu_tlb_batch));
        } else {
            abort("Not supported in pipelined");
        }
    }
}

void evictor::_rdma_update_pte_pipelined(
    size_t cpu_id,
    base_page_slice_t& slice_active,
    base_page_slice_t& slice_clean,
    base_page_slice_t& slice_rdma){
    // Loop over slice_rdma to update PTE (clean_slice)
    base_page_slice_t::iterator next;
    auto page = slice_rdma.begin();
    auto ret = state::OK_SKIP;
    size_t i = 0;
    while (page != slice_rdma.end()){
        if (opt_tlb_flush_mode == "patr")
            flush_tlb_check_and_flush(cpu_id);
        mmu::phys paddr = ipt.lookup(*page);
        uintptr_t offset = va_to_offset(page->va);
        mmu::pt_element<base_page_level> old = page->ptep.read();

        // Only valid, not accessed and dirty will be written
        // Clean will go directly to clean slice
        // TODO: Support NO_SG
        if (check_skip(old) || !is_ddc(page->va)){
            ret = state::OK_SKIP;
        } else if (old.valid()){
            // We will flush the entry anyway
            if (old.accessed()) {
                ret = state::OK_SKIP;
            } else if (old.dirty()) {
                auto new_pte = old;
                new_pte.set_dirty(false);
                if (!page->ptep.compare_exchange(old, new_pte)) {
                    ret = state::PTE_UPDATE_FAIL;
                } else {
                    ret = state::OK_PUSHED;
                }
            } else {
                ret = state::OK_FINISH;
            }
        }
        // State machine
        switch(ret){
            case state::OK_SKIP:
                next = slice_rdma.erase(page);
                slice_active.push_back(*page);
                page = next;
                break;
            case state::OK_PUSHED:
                // Keep the page in slice_rdma because we have not 
                // ack the rdma batch yet
                (*_percpu_paddrs)[i] = paddr;
                // Kernel virtual address
                (*_percpu_virts)[i] = mmu::phys_to_virt(old.addr());
                (*_percpu_offsets)[i++] = offset;
                page++;
                break; 
            case state::OK_FINISH:
                next = slice_rdma.erase(page);
                slice_clean.push_back(*page);
                page = next;
                break;
            case state::PTE_UPDATE_FAIL:
                // Retry
                continue;
            default:
                abort("noreach");
        }
    }

    // Now everything should reside in _percpu_rdma_batch
    // The slice_rdma should be empty.
    assert(i == slice_rdma.size());
}

void evictor::_rdma_send_pipelined(
    size_t cpu_id,
    base_page_slice_t& slice_rdma){
    // Actually sending RDMA for the whole batch
    // Only signal the last one

    // Let's just operate on qp directly
    // Ignore SG and just manually put them to an sge and ack once
    
    // TODO: Support local DRAM driver! Now hardcoded to be RDMA!
    // NOTE madv uses different qp as here
    if (slice_rdma.size()){
        SCOPE_LOCK(_percpu_mutex[cpu_id]);
        auto &rq = _percpu_rq[cpu_id];
        // We expect to work with only one entry 
        assert(_percpu_pushed[cpu_id] == 0);
        SCOPE_LOCK(preempt_lock);

        rq.push_multiple(0, 
            slice_rdma.size(), 
            (*_percpu_paddrs), 
            (*_percpu_virts), 
            (*_percpu_offsets), 
            base_page_size);
        // DROP_LOCK(preempt_lock){
        //     WITH_LOCK(debug_lock){
        //         debug_early_u64("cpu_id ", cpu_id);
        //         debug_early_u64("size ", slice_rdma.size());
        //         debug_early_u64("S addr ", (*_percpu_paddrs)[0]);
        //         debug_early_u64("S1 addr ", (*_percpu_paddrs)[1]);
        //         debug_early_u64("E addr ", ((*_percpu_paddrs)[slice_rdma.size() - 1]));
        //     }
        // }
        _percpu_pushed[cpu_id] ++;
        assert(!(*_percpu_rdma_batch)->size());
        (*_percpu_rdma_batch)->splice(
            (*_percpu_rdma_batch)->end(), slice_rdma);
    } 
}

void evictor::_rdma_ack_pipelined(
    size_t cpu_id,
    base_page_slice_t& slice_clean){
    // Only ack the last one in the batch and move all to the 
    // clean slice
    // Poll first for the last
    if ((*_percpu_rdma_batch) -> size()){
        SCOPE_LOCK(_percpu_mutex[cpu_id]);
        // Should only contain one
        assert(_percpu_pushed[cpu_id] == 1);
        auto &rq = _percpu_rq[cpu_id];
        
        if (opt_rdma_mode == "selective"){
            uintptr_t token; 
            int polled = rq.poll(&token, 1);
            while (!polled){
                if (opt_tlb_flush_mode == "patr"){
                    flush_tlb_check_and_flush(cpu_id);
                }
                sched::cpu::schedule();
                polled += rq.poll(&token, 1);
            }
            assert(token == ipt.lookup(*(
                (*_percpu_rdma_batch)->rbegin()
            )));
        } else {
            // Let's just try poll all first
            size_t n = (*_percpu_rdma_batch)->size();
            uintptr_t tokens[n];
            size_t polled = rq.poll(tokens, n);
            while (polled < n){
                if (opt_tlb_flush_mode == "patr"){
                    flush_tlb_check_and_flush(cpu_id);
                }
                sched::cpu::schedule();
                polled += rq.poll(tokens + polled, n - polled);
            }
            assert(tokens[0] == ipt.lookup(*((*_percpu_rdma_batch)->begin())));
            assert(tokens[n - 1] == ipt.lookup(*((*_percpu_rdma_batch)->rbegin())));
        }

        _percpu_pushed[cpu_id] --;

        // Move all page to slice_clean for further 
        slice_clean.splice(slice_clean.end(), (**_percpu_rdma_batch));
    }
}

size_t evictor::_reclaim_from_slice_clean_pipelined(
    size_t cpu_id,
    base_page_slice_t& slice_active,
    base_page_slice_t& slice_clean){
    // Loop over _percpu_rdma_batch (evict slice) and reclaim 
    // those which satisfies the condition
    base_page_slice_t::iterator next;
    size_t freed = 0;
    auto page = slice_clean.begin();
    while (page != slice_clean.end()){
        if (opt_tlb_flush_mode == "patr")
            flush_tlb_check_and_flush(cpu_id);
        auto pte = page->ptep.read();  
        if (pte.empty()){
            // TODO: Reexamine what is happening here?
            // where will the page go?
            page++;
            abort("pte empty not handled");
            continue;
        }
        assert(pte.valid());
        if (pte.accessed()) {
            next = slice_clean.erase(page);
            slice_active.push_back(*page);
            page = next;
        } else {
            assert(!pte.accessed());
            assert(!pte.dirty());
            assert(page->va);
            // TODO: FIX This when no SG!
            auto pte_new = remote_pte_full(page->ptep);
            if (!page->ptep.compare_exchange(pte, pte_new)) {
                // retry
                continue;
            }
            next = slice_clean.erase(page);
            freed += base_page_size;
            page->st = page_status::UNTRACK;
            page_list.free_not_used_page(*page);
            page = next;
        }
    }
    return freed;
}

size_t evictor::_finalize_pipelined(size_t cpu_id){
    base_page_slice_t slice_active;
    base_page_slice_t slice_clean;

    // Ack TLB Flush and move to slice_active
    if (opt_tlb_flush_mode == "patr"){
        SCOPE_LOCK(preempt_lock);
        int done = mmu::flush_tlb_cooperative_check();
        if (!done){
            // This fallback will be very costly and 
            // can be only covered if the time waiting 
            // for RDMA is high enough
            mmu::flush_tlb_all();
        }
    } else if (opt_tlb_flush_mode == "batch"){
        mmu::flush_tlb_all_ack_pipelined(cpu_id);
    } else {
        abort("Unknown async reclaim mode");
    }
    slice_active.splice(slice_active.end(), (**_percpu_tlb_batch));

    // Ack RDMA and move to slice_clean
    _rdma_ack_pipelined(cpu_id, slice_clean);

    // Reclaim slice_clean
    auto freed = _reclaim_from_slice_clean_pipelined(
        cpu_id, slice_active, slice_clean);

    // Do some check before we put
    assert(slice_clean.empty());
    assert((*_percpu_tlb_batch)->empty());
    assert((*_percpu_rdma_batch)->empty());

    // Put slice_active back
    WITH_LOCK(preempt_lock){
        bool back = false;
        size_t active_list_id = page_list.get_active_list(
            cpu_id, false, true, &back);
        page_list.push_pages_active(active_list_id, slice_active, false);
    }
    assert(slice_active.empty());
    // Clean up the buffer
    ::memory::clear_reclaim_buf();
    return freed;
}

size_t evictor::_reclaim_pipelined(size_t cpu_id){
    // TODO: We need to check the reclaim efficiency.
    // i.e. how many pages are filtered in each batch

    // Ignore the clean list for this case

    // TODO: Right now we use two temporary staging buffer which can 
    // cause 2x more looping cost. We should consider removing them
    base_page_slice_t slice_active;
    base_page_slice_t slice_clean;
    base_page_slice_t slice_tlb;
    base_page_slice_t slice_rdma;
    bool back = true;
    
    // Slice first
    WITH_LOCK(preempt_lock){
        size_t active_list_id = page_list.get_active_list(
            cpu_id, true, true, &back);
        page_list.slice_pages_active(
            active_list_id, slice_active, ddc::pipeline_batch_size);
    }
    
    // Update PTE, update slice_active and slice_tlb inside
    _update_pte_pipelined(slice_active, slice_tlb, slice_rdma);

    // Ack TLB Batch
    _tlb_flush_ack_pipelined(cpu_id, slice_active, slice_rdma);

    // Flush this Batch
    _tlb_flush_send_pipelined(cpu_id, slice_tlb);

    // Check the PTE after flush
    _rdma_update_pte_pipelined(cpu_id, 
        slice_active, slice_clean, slice_rdma);
    
    // For static LRU, put slice_active back first
    if (opt_lru_mode == "static_lru"){
        WITH_LOCK(preempt_lock){
            bool back = false;
            size_t active_list_id = page_list.get_active_list(
                cpu_id, false, true, &back);
            page_list.push_pages_active(active_list_id, slice_active, false);
        }
    }


    // Ack RDMA
    _rdma_ack_pipelined(cpu_id, slice_clean);

    // Process for RDMA
    _rdma_send_pipelined(cpu_id, slice_rdma);

    // Free pages
    auto freed = _reclaim_from_slice_clean_pipelined(cpu_id, 
        slice_active, slice_clean);

    // Make sure all temporary buffers are empty 
    assert(slice_clean.empty());
    assert(slice_tlb.empty());
    assert(slice_rdma.empty());

    // Push back
    WITH_LOCK(preempt_lock){
        bool back = false;
        size_t active_list_id = page_list.get_active_list(
            cpu_id, false, true, &back);
        page_list.push_pages_active(active_list_id, slice_active, false);
    }
    assert(slice_active.empty());
    return freed;
}

size_t evictor::request_memory_pipelined(size_t s, bool hard) {
    size_t freed = 0;
    SCOPE_LOCK(migration_lock);
    auto cpu_id = sched::cpu::current() -> id;
    memory::pressure p = pressure_level();
    while (freed < s && p > memory::pressure::NORMAL){
        freed += _reclaim_pipelined(cpu_id);
    }
    // Finalize
    freed += _finalize_pipelined(cpu_id);
    return freed;
}

// Initialization part

static evictor *_evictor;

void evictor_init() {
    if (options::no_eviction) return;
    for (size_t i = 0; i < sched::cpus.size(); ++i){
        percpu_remote_queues[i].add_push(current_max_evict);
        percpu_remote_queues[i].setup();
    }

    _evictor = new evictor;
}

void eviction_get_stat(size_t &fetch_total, size_t &push_total) {
    size_t local_fetch_total = 0, local_push_total = 0;
    size_t local_fetch = 0, local_push = 0;
    for (size_t i = 0; i< sched::cpus.size(); ++i){
        percpu_remote_queues[i].get_stat(local_fetch, local_push);
        local_fetch_total += local_fetch;
        local_push_total += local_push;
    }
    fetch_total = local_fetch_total;
    push_total = local_push_total;
}

}  // namespace ddc
