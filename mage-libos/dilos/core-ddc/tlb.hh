#pragma once

#include <osv/types.h>

#include <array>
#include <boost/circular_buffer.hpp>
#include <ddc/memory.hh>
#include <osv/mmu.hh>
#include <osv/spinlock.h>
#include <osv/percpu.hh>
#include <ddc/stat.hh>
#include <ddc/page.hh>

extern std::string opt_tlb_flush_mode;
extern percpu<u64> tick_tlb_coop;
extern percpu<u64> cnt_tlb_coop;
extern percpu<u64> tick_tlb_nocoop;
extern percpu<u64> cnt_tlb_nocoop;

namespace ddc {

static constexpr size_t n_token_queues = 2;

enum {
    tlb_flush_cooperative_done = 0,
    tlb_flush_cooperative_undone = 1,
};
struct tlb_token_queue_status {
    // Switched when full
    size_t current_token_queues;
    // 0 done, 1 undone;
    size_t cooperative_tlb_flush_status[n_token_queues];
};

// This class will be used by madvise. Each madvise owns 
// this class to prevent interference with the class of 
// the evictior
struct tlb_flusher_madv {
    public:
    static constexpr size_t max_tokens = 512;

    boost::circular_buffer<uintptr_t> tokens;
    tlb_flusher_madv() : tokens(max_tokens) {}

    virtual void tlb_flush_after(uintptr_t token) = 0;

    void tlb_push(uintptr_t token) {
        if (tokens.full()) tlb_flush();
        tokens.push_back(token);
    }
    inline bool tlb_empty() { return tokens.empty(); }
    size_t tlb_flush() {
        if (tlb_empty()) {
            return 0;
        }
        mmu::flush_tlb_all();
        size_t tokens_size = tokens.size();
        for (auto i = 0u; i < tokens_size; ++i) {
            auto token = tokens.front();
            tokens.pop_front();
            tlb_flush_after(token);
        }
        return tokens_size;
    }
};

struct tlb_flusher {
   public:
    static constexpr size_t max_tokens = 64;

    tlb_flusher();
    // tlb_flush_after should by itself provide thread-safety.
    virtual void tlb_flush_after(uintptr_t token) = 0;

    void tlb_push(uintptr_t token);

    bool tlb_empty(unsigned cpu_id);

    bool tlb_full(unsigned cpu_id);

    size_t tlb_flush(unsigned cpu_id);

    size_t tlb_flush_locked(unsigned cpu_id);

    inline void set_tlb_cooperative_undone();
    
    inline void set_tlb_cooperative_done();
    inline size_t next_token_queue();
};

};  // namespace ddc
