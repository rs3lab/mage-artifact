#pragma once

#include <osv/spinlock.h>
#include <osv/ticketlock.h>
#include <osv/mcs_lock.h>

#include <ddc/memory.hh>

#include <ddc/stat.hh>

// Available Options:
//  - static_lru: LRU, shard n list statically using cpu_id (put to front after flush)
//  - static_fifo: fifo, shard n list statically using cpu_id (put to back after flush)
//  - lb: slice from the longest and push to the shortest
//  - rr: both sync and async reclaim threads follows round robin
//  - rr_spec_async: static for sync reclaim, rr for async reclaim
//  - dynamic: dynamic adjust
extern std::string opt_lru_mode;

namespace ddc {

enum class page_status {
    UNTRACK,
    TRACK_PERCPU,
    TRACK_ACTIVE,
    TRACK_CLEAN,
    TRACK_SLICE,
    LOCKED,
    DONT_NEED
};

namespace bi = boost::intrusive;
template <int N>
struct page_t {
    uintptr_t va;
    uint64_t vec;  // this is for sg
    union {
        mmu::hw_ptep<N> ptep;
        void *meta;  // This is for app-driven metadata
    };
    bi::list_member_hook<> hook;

    // to read/modify this, needs active_list lock or initialization (ownership)
    page_status st;
    
    // This is a temporary fix. pageout, dontneed and mlock needs deletion 
    // from active or clean directly. This field is used to record which list 
    // it is on
    int list_id;

    page_t() : va(0), vec(0), meta(NULL), st(page_status::UNTRACK), list_id(-1) {}

    // this require ownership
    void reset() {
        va = 0;
        vec = 0;
        meta = NULL;
        st = page_status::UNTRACK;
        list_id = -1;
    }
    page_t(const page_t &) = delete;
    page_t &operator=(const page_t &) = delete;
};
template <int N>
using page_slice_t = bi::list<
    page_t<N>,
    bi::member_hook<page_t<N>, bi::list_member_hook<>, &page_t<N>::hook>,
    bi::constant_time_size<true>>;

using base_page_t = page_t<base_page_level>;
using base_page_slice_t = page_slice_t<base_page_level>;

/* IPT */
template <unsigned level>
class ipt_t {
    const size_t page_size = mmu::page_size_level(level);

   public:
    ipt_t(mmu::phys end) : _end(end), pages(end / page_size) {
        assert(end % page_size == 0);
    }

    page_t<level> &lookup(mmu::phys addr) {
        assert(addr < _end);

        return pages[addr / page_size];
    }

    page_t<level> &lookup(void *addr) {
        return lookup(mmu::virt_to_phys(addr));
    }

    mmu::phys lookup(page_t<level> &page) {
        auto addr = (&page - pages.data()) * page_size;
        assert(addr < _end);
        return addr;
    }

   private:
    const mmu::phys _end;
    std::vector<page_t<level>> pages;
};

extern ipt_t<base_page_level> ipt;

// TODO: Make this also configurable
constexpr size_t max_queues = 4;

extern spinlock_t active_list_locks[max_queues];
extern spinlock_t clean_list_locks[max_queues];

STAT_ELAPSED(ddc_slice_page_active);
STAT_ELAPSED(ddc_slice_page_clean);
STAT_ELAPSED(ddc_slice_inner_full);

class page_list_t {
   public:
    page_list_t() {
        for(size_t i = 0 ; i < ddc::max_cpu; i++){
            rr_history[i] = i % max_queues;
        }
    }

    // get the active list. slice == true means we try to slice
    size_t get_active_list(size_t hint_list, bool slice, bool reclaim, bool *back);
    size_t get_clean_list(size_t hint_list, bool slice, bool reclaim);

    // used in first fault / remote fault
    base_page_t &get_page(void *paddr, uintptr_t va,
                          mmu::hw_ptep<base_page_level> &ptep) {
        base_page_t &pg = ipt.lookup(paddr);
        assert(pg.st == page_status::UNTRACK);
        pg.st = page_status::TRACK_PERCPU;
        pg.va = va;
        pg.ptep = ptep;
        assert(!pg.hook.is_linked());
        return pg;
    }

    void free_not_used_page(base_page_t &page) {
        page.st = page_status::UNTRACK;
        free_page(page);
    }

    void free_paddr(mmu::phys paddr) {
        auto &page = ipt.lookup(paddr);
        assert(page.st != page_status::UNTRACK);
        // This read is not protected by the lock. Can be problematic here
        // Just a temporary workaround to make pageout compile
        if (page.st == page_status::TRACK_ACTIVE ||
            page.st == page_status::TRACK_CLEAN ||
            page.st == page_status::LOCKED) {
            pop_page(page);
            free_not_used_page(page);

        } else {
            // DONT_NEED
            // TRACK_PERCPU
            // TRACK_SLICE
            page.st = page_status::DONT_NEED;
        }
    }

    void push_pages_active(size_t list_id, base_page_slice_t &pages, bool back) {
        SCOPE_LOCK(active_list_locks[list_id]);
        push_pages_inner(pages, actives[list_id], list_id, page_status::TRACK_ACTIVE, back);
        sizes[list_id] = actives[list_id].size();
    }

    void slice_pages_active(size_t list_id, base_page_slice_t &pages, size_t n) {
        SCOPE_LOCK(active_list_locks[list_id]);
        slice_pages_inner(pages, n, actives[list_id]);
        sizes[list_id] = actives[list_id].size();
    }
    void slice_pages_clean(size_t list_id, base_page_slice_t &pages, size_t n) {
        SCOPE_LOCK(clean_list_locks[list_id]);
        STAT_ELAPSED_FROM_WRAP(ddc_slice_page_clean);
        slice_pages_inner(pages, n, cleans[list_id]);
        STAT_ELAPSED_TO_WRAP(ddc_slice_page_clean);
    }
    void push_pages_both(size_t active_list_id, base_page_slice_t &pages_active, bool back,
                         size_t clean_list_id, base_page_slice_t &pages_dirty) {
        WITH_LOCK(active_list_locks[active_list_id]){
            push_pages_inner(pages_active, actives[active_list_id], active_list_id, page_status::TRACK_ACTIVE, back);
            sizes[active_list_id] = actives[active_list_id].size();
        }
        WITH_LOCK(clean_list_locks[clean_list_id]){
            push_pages_inner(pages_dirty, cleans[clean_list_id], clean_list_id, page_status::TRACK_CLEAN, true);
        }
    }
    void erase_from_active(base_page_t &page) {
        int list_id = page.list_id;
        actives[list_id].erase(actives[list_id].iterator_to(page));
    }
    void erase_from_clean(base_page_t &page) {
        int list_id = page.list_id;
        cleans[list_id].erase(cleans[list_id].iterator_to(page));
    }

    void pop_page_locked(base_page_t &page) {
        int list_id = page.list_id;
        auto it = base_page_slice_t ::s_iterator_to(page);
        switch (page.st) {
            case page_status::TRACK_ACTIVE:
                actives[list_id].erase(it);
                break;
            case page_status::TRACK_CLEAN:
                cleans[list_id].erase(it);
                break;
            default:
                abort();
        }
    }

    void pop_page(base_page_t &page) {
        // TODO: check inpercpu
        int list_id = page.list_id;
        if (list_id == -1) abort();
        SCOPE_LOCK(active_list_locks[list_id]);
        SCOPE_LOCK(clean_list_locks[list_id]);
        pop_page_locked(page);
    }

   private:
    void free_page(base_page_t &page);

    inline void push_pages_inner(base_page_slice_t &pages,
                                 base_page_slice_t &list, size_t list_id, page_status st, bool back) {
        auto page = pages.begin();

        while (page != pages.end()) {
            if (page->st == page_status::DONT_NEED) {
                // this page marked need free;
                auto next = pages.erase(page);
                page->st = page_status::UNTRACK;
                free_page(*page);
                page = next;
            } else if (page->st == page_status::LOCKED) {
                // just move out
                page = pages.erase(page);
            } else {
                page->st = st;
                page->list_id = list_id;
                page++;
            }
        }
        if (back)
            list.splice(list.end(), pages);
        else 
            list.splice(list.begin(), pages);

    }

    inline void slice_pages_inner(base_page_slice_t &pages, size_t n,
                                  base_page_slice_t &list) {
        size_t i;
        auto end = list.begin();
        for (i = 0; i < n && end != list.end(); ++i) {
            if (end->st == page_status::DONT_NEED) {
                auto next = list.erase(end);

                --i;
                free_page(*end);

                end = next;
            } else {
                end->st = page_status::TRACK_SLICE;
                end->list_id = -1;
                ++end;
            }
        }
        if (i) {
            pages.splice(pages.end(), list, list.begin(), end, i);
        }
    }
    
    inline void slice_pages_inner_active(base_page_slice_t &pages, size_t n,
                                  base_page_slice_t &list, size_t list_id) {
        size_t i = 0;
        base_page_slice_t tmp;
        WITH_LOCK(active_list_locks[list_id]){
            STAT_ELAPSED_FROM_WRAP(ddc_slice_inner_full);
            auto end = list.begin();
            auto start = list.begin();
            std::advance(end, n);
            if (start != end) {
                tmp.splice(tmp.end(), list, start, end);
            }
            STAT_ELAPSED_TO_WRAP(ddc_slice_inner_full);
        }
        // TODO: remove the second splice
        auto end = tmp.begin();
        while (end != tmp.end()) {
            if (end->st == page_status::DONT_NEED) {
                auto next = tmp.erase(end);
                free_page(*end);
                end = next;
            } else {
                end->st = page_status::TRACK_SLICE;
                end->list_id = -1;
                ++end;
                ++i;
            }
        }
        if (i) {
            pages.splice(pages.end(), tmp, tmp.begin(), end, i);
        }
    }

   private:
    // Workflow:
    // Comb 1: LRU + Scan flush
    //      page at the back of active:
    //          - access bit set: clear, flush, circulate from active front
    //          - dirty bit set, active unset: RDMA writeback, circulate from 
    //            active front if access set. Otherwise circulate from clean
    //            front.
    //          - no bit set: circulate from clean front
    //      page at the back of clean:
    //          - access bit set: circulate from the active front
    //          - access bit unset: reap
    //
    // Comb 2: LRU + Scan no flush
    //      page at the back of active:
    //          - access bit set: clear, circulate from active front
    //          - access bit unset: flush, RDMA writeback if dirty (?), reap if 
    //            access bit unset (?)
    //
    // Comb 3: FIFO + Scan flush / no flush
    //      page at the back of active:
    //          - clear, flush, RDMA write if dirty (?), reap if access bit unset (?)
    // TODO: Reexamine the correctness
    // TODO: Pipelining

    base_page_slice_t actives[max_queues];
    base_page_slice_t cleans[max_queues];

    // To store the size of active list. Not guaranteed to be precise. 
    // Just for a rough load balance
    size_t sizes[max_queues];
    std::array<uint64_t, ddc::max_cpu> rr_history;
    const int64_t time_thres = 10000; // 10us
    const int64_t log_thres = 10000000; // 10ms
    const uint64_t tput_thres = 10; // 400k/s
    const uint64_t fifo_thres = 45; // 3m/s
};
extern page_list_t page_list;

void insert_page_buffered(base_page_t &page,
                          bool try_flush = true);  // needs preempt_lock

void try_flush_buffered();  // needs preempt_lock

void eviction_get_stat(size_t &fetch_total, size_t &push_total);

}  // namespace ddc