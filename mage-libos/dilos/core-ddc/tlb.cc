#include <osv/types.h>

#include "tlb.hh"
#include <osv/percpu.hh>

PERCPU(u64, tick_tlb_coop);
PERCPU(u64, cnt_tlb_coop);
PERCPU(u64, tick_tlb_nocoop);
PERCPU(u64, cnt_tlb_nocoop);

TRACEPOINT(trace_ddc_async_tlb_coop_check, "%d", size_t);
namespace ddc {
STAT_ELAPSED(ddc_tlb_push);
STAT_ELAPSED(ddc_tlb_push_inside_lock);
STAT_ELAPSED(ddc_tlb_flush_locked);
STAT_ELAPSED(ddc_tlb_flush);
STAT_ELAPSED(ddc_tlb_flush_all);
STAT_AVG(ddc_tlb_flush_size);
STAT_ELAPSED(ddc_tlb_cooperative);
STAT_ELAPSED(ddc_tlb_flush_all_in_flush_func);
STAT_ELAPSED(ddc_tlb_flush_all_on_behalf);

// These are the static resources shared by all the classes inherited from tlb_flush 
// class. Otherwise suppose A, B both inherit the tlb_flush class and both of them 
// create instances, A and B have their individual queue resource. Once A or B is 
// destructed, the pages in the queue will be lost. This is unacceptable

static PERCPU(struct tlb_token_queue_status, queue_status);
std::array<spinlock_t, max_cpu> _percpu_token_lock;
std::array<boost::circular_buffer<uintptr_t>, max_cpu> _percpu_tokens[n_token_queues];
std::array<base_page_slice_t, ddc::max_cpu> _per_cpu_active_buffer;
std::array<base_page_slice_t, ddc::max_cpu> _per_cpu_clean_buffer;

tlb_flusher::tlb_flusher() {
    for (size_t i = 0; i < sched::cpus.size(); i++){
        for(size_t j = 0; j < n_token_queues; j++)
            _percpu_tokens[j][i] = boost::circular_buffer<uintptr_t>(max_tokens);
    }   
}

void tlb_flusher::tlb_push(uintptr_t token) {
    // Can explode
    STAT_ELAPSED_FROM_WRAP(ddc_tlb_push);
    assert(!sched::thread::current()->migratable());
    unsigned cpu_id = sched::cpu::current()->id;
    //SCOPE_LOCK(_percpu_token_lock[cpu_id]);
    SCOPE_LOCK(preempt_lock);
    STAT_ELAPSED_FROM_WRAP(ddc_tlb_push_inside_lock);
    if (tlb_full(cpu_id)) tlb_flush_locked(cpu_id);
    if (opt_tlb_flush_mode == "batch" && 
        _percpu_tokens[queue_status->current_token_queues][cpu_id].full()){
            queue_status->current_token_queues += 1;
    }
    _percpu_tokens[queue_status->current_token_queues][cpu_id].push_back(token);
    STAT_ELAPSED_TO_WRAP(ddc_tlb_push_inside_lock);
    STAT_ELAPSED_TO_WRAP(ddc_tlb_push);
}

bool tlb_flusher::tlb_empty(unsigned cpu_id) { 
    if (opt_tlb_flush_mode == "batch"){
        return _percpu_tokens[0][cpu_id].empty();
    }
    for (size_t i = 0; i < n_token_queues; i++){
        if (!_percpu_tokens[i][cpu_id].empty())
            return false;
    }
    return true;
}

bool tlb_flusher::tlb_full(unsigned cpu_id) {
    if (opt_tlb_flush_mode == "batch") {
        return _percpu_tokens[n_token_queues - 1][cpu_id].full();
    }
    return _percpu_tokens[queue_status->current_token_queues][cpu_id].full();
}

size_t tlb_flusher::tlb_flush(unsigned cpu_id) {
    // Here we don't allow the token queue to be altered when TLB is being flushed
    assert(!sched::thread::current()->migratable());
    unsigned current_cpu = sched::cpu::current() -> id;
    STAT_ELAPSED_FROM_WRAP(ddc_tlb_flush);
    // _percpu_token_lock[cpu_id].lock();
    SCOPE_LOCK(preempt_lock);
    if (opt_tlb_flush_mode == "batch"){
        size_t tokens_size = 0;
        for (size_t i = 0; i <= queue_status->current_token_queues; i++){
            tokens_size += _percpu_tokens[i][cpu_id].size();
        }
        if (!tokens_size) {
            return 0;
        }
        mmu::flush_tlb_all();
        for (auto i = 0u; i < tokens_size; ++i) {
            auto q_id = i / max_tokens;
            auto token = _percpu_tokens[q_id][cpu_id].front();
            _percpu_tokens[q_id][cpu_id].pop_front();
            tlb_flush_after(token);
        }
        queue_status->current_token_queues = 0;
        return tokens_size;
    }
    
    // On behalf of the application threads
    bool on_behalf = current_cpu != cpu_id;

    size_t current_id = on_behalf ?  (queue_status.for_cpu(sched::cpus[cpu_id]))->current_token_queues 
        : queue_status->current_token_queues;
    size_t tokens_size = 0;
    for (size_t i = 0; i < n_token_queues; i++){
        tokens_size += _percpu_tokens[i][cpu_id].size();
    }
    STAT_AVG_ADD(ddc_tlb_flush_size, tokens_size);
    if (!tokens_size) {
        //_percpu_token_lock[cpu_id].unlock();
        STAT_ELAPSED_TO_WRAP(ddc_tlb_flush);
        return 0;
    }
    if (on_behalf){
        STAT_ELAPSED_FROM_WRAP(ddc_tlb_flush_all_on_behalf);
        mmu::flush_tlb_on_behalf(cpu_id);
        STAT_ELAPSED_TO_WRAP(ddc_tlb_flush_all_on_behalf);
    } else {
        STAT_ELAPSED_FROM_WRAP(ddc_tlb_flush_all_in_flush_func);
        mmu::flush_tlb_all();
        STAT_ELAPSED_TO_WRAP(ddc_tlb_flush_all_in_flush_func);
    }
    tokens_size = 0;
    size_t next_id = (current_id + 1) % n_token_queues;
    for(size_t iter_q = 0; iter_q < n_token_queues; iter_q ++){
        auto tmp = _percpu_tokens[next_id][cpu_id].size();
        for (auto i =0u; i < tmp; ++i){
            auto token = _percpu_tokens[next_id][cpu_id].front();
            _percpu_tokens[next_id][cpu_id].pop_front();
            tlb_flush_after(token);
        }
        next_id = (next_id + 1) % n_token_queues;
        tokens_size += tmp;
    }
    size_t *status_array = on_behalf ? (queue_status.for_cpu(sched::cpus[cpu_id]) ->cooperative_tlb_flush_status)
        : queue_status->cooperative_tlb_flush_status;
    memset(status_array, 0, sizeof(size_t) * n_token_queues);
    //_percpu_token_lock[cpu_id].unlock();
    STAT_ELAPSED_TO_WRAP(ddc_tlb_flush);
    return tokens_size;
}


size_t tlb_flusher::tlb_flush_locked(unsigned cpu_id) {
    auto tick1 = processor::ticks(); 
    assert(!sched::thread::current()->migratable());
    // Here we don't allow the token queue to be altered when TLB is being flushed
    STAT_ELAPSED_FROM_WRAP(ddc_tlb_flush_locked);
    if (opt_tlb_flush_mode == "batch"){
        size_t tokens_size = 0;
        // Just a check on the correctness
        assert(queue_status->current_token_queues == 1);
        for (size_t i = 0; i <= queue_status->current_token_queues; i++){
            tokens_size += _percpu_tokens[i][cpu_id].size();
        }
        if (!tokens_size) {
            return 0;
        }
        mmu::flush_tlb_all();
        for (auto i = 0u; i < tokens_size; ++i) {
            auto q_id = i / max_tokens;
            auto token = _percpu_tokens[q_id][cpu_id].front();
            _percpu_tokens[q_id][cpu_id].pop_front();
            tlb_flush_after(token);
        }
        queue_status->current_token_queues = 0;
        return tokens_size;
    }

    size_t tokens_size = _percpu_tokens[queue_status->current_token_queues][cpu_id].size();
    if (!tokens_size) {
        STAT_ELAPSED_TO_WRAP(ddc_tlb_flush_locked);
        return 0;
    }

    // For app threads, do cooperative TLB flush when possible
    // For non app threads, don't do cooperative TLB flush.
    assert(n_token_queues == 2);
    // Only put the request. No wait.
    // Use another set of csd just for cooperative tlb flush
    // Depends on whether the next has finished or not
    
    // NOTE: Here next_id is the same as prev id because of 2
    size_t next_id = (queue_status->current_token_queues + 1) % n_token_queues;

    if (queue_status->cooperative_tlb_flush_status[next_id] == tlb_flush_cooperative_undone){
        // Check whether this is done or not
        int done = mmu::flush_tlb_cooperative_check();
        trace_ddc_async_tlb_coop_check(done);
        if (done){
            STAT_ELAPSED_FROM_WRAP(ddc_tlb_cooperative);
            auto tmp = _percpu_tokens[next_id][cpu_id].size();
            for (auto i =0u; i < tmp; ++i){
                auto token = _percpu_tokens[next_id][cpu_id].front();
                _percpu_tokens[next_id][cpu_id].pop_front();
                tlb_flush_after(token);
            }
            tokens_size = tmp;
            mmu::flush_tlb_cooperative();
            set_tlb_cooperative_undone();
            next_token_queue();
            set_tlb_cooperative_done();
            STAT_ELAPSED_TO_WRAP(ddc_tlb_cooperative);
            *tick_tlb_coop += processor::ticks() - tick1;
            *cnt_tlb_coop += 1;
        } else {
            // Unfortunately no more space for cooperative TLB flush so ipi based tlb flush
            STAT_ELAPSED_FROM_WRAP(ddc_tlb_flush_all);
            mmu::flush_tlb_all();
            STAT_ELAPSED_TO_WRAP(ddc_tlb_flush_all);
            tokens_size = 0;
            for(size_t iter_q = 0; iter_q < n_token_queues; iter_q ++){
                auto tmp = _percpu_tokens[next_id][cpu_id].size();
                for (auto i =0u; i < tmp; ++i){
                    auto token = _percpu_tokens[next_id][cpu_id].front();
                    _percpu_tokens[next_id][cpu_id].pop_front();
                    tlb_flush_after(token);
                }
                next_id = (next_id + 1) % n_token_queues;
                tokens_size += tmp;
            }
            memset(queue_status->cooperative_tlb_flush_status, 0, sizeof(size_t) * n_token_queues);
            *tick_tlb_nocoop += processor::ticks() - tick1;
            *cnt_tlb_nocoop += 1;
        }
    } else {
        STAT_ELAPSED_FROM_WRAP(ddc_tlb_cooperative);
        mmu::flush_tlb_cooperative();
        set_tlb_cooperative_undone();
        next_token_queue();
        STAT_ELAPSED_TO_WRAP(ddc_tlb_cooperative);
        *tick_tlb_coop += processor::ticks() - tick1;
        *cnt_tlb_coop += 1;
    }
    STAT_ELAPSED_TO_WRAP(ddc_tlb_flush_locked);
    return tokens_size;
}

inline void tlb_flusher::set_tlb_cooperative_undone(){
    queue_status->cooperative_tlb_flush_status[queue_status->current_token_queues] = 
        tlb_flush_cooperative_undone;
}
inline void tlb_flusher::set_tlb_cooperative_done(){
    queue_status->cooperative_tlb_flush_status[queue_status->current_token_queues] =
        tlb_flush_cooperative_undone;
}
inline size_t tlb_flusher::next_token_queue(){
    queue_status->current_token_queues = (queue_status->current_token_queues + 1) % n_token_queues;
    return queue_status->current_token_queues;
}

};  // namespace ddc
