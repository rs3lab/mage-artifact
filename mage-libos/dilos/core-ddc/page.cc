#include <ddc/page.hh>

#include <osv/spinlock.h>

#include <ddc/memory.hh>
#include <ddc/phys.hh>
#include <ddc/stat.hh>
#include <osv/mmu.hh>
#include <osv/percpu.hh>
#include <osv/preempt-lock.hh>
#include <osv/mempool.hh>

#include <vector>
namespace ddc {

ipt_t<base_page_level> ipt(mmu::virt_to_phys(memory::get_phys_max()));

/* Page List */
spinlock_t active_list_locks[max_queues];
spinlock_t clean_list_locks[max_queues];
page_list_t page_list;

static PERCPU(base_page_slice_t *, active_page_buffer);
static sched::cpu::notifier _notifier([]() {
    *active_page_buffer = new base_page_slice_t;
});

extern std::atomic<uint64_t> n_faultin;
PERCPU(uint64_t, percpu_fault_in_snapshot);
PERCPU(osv::clock::uptime::time_point, percpu_last_tp);
PERCPU(osv::clock::uptime::time_point, percpu_last_log);
PERCPU(uint64_t, percpu_last_tput);

constexpr size_t active_list_buffer_size = 32;

TRACEPOINT(trace_ddc_dynamic_stat, "dur=%lu, tput=%lu, nq=%lu", uint64_t, uint64_t, uint64_t);

void page_list_t::free_page(base_page_t &page) {
    assert(!page.hook.is_linked());
    assert(page.st == page_status::UNTRACK);
    auto paddr = ipt.lookup(page);
    void *vaddr = mmu::phys_to_virt(paddr);
    page.reset();
    // Instead of freeing the page to L1, the page should go to the 
    // free_page_ranges. Does not work, performance collapse.
    //::memory::free_page_to_range(vaddr);

    // TODO: Add local batch for each async thread
    // Workflow:
    // Comb1: No global L2 + no sync: batch, always to range
    // Comb2: No global L2 + response: sync to L1, async to batch to range
    // Comb3: No global L2 + normal: always to L1
    // Comb4: global L2 + no sync: batch, always to L2
    // Comb5: global L2 + response: sync to L1, async to batch to L2
    // Comb6: global L2 + normal: always to L1 (L2?)

    // TODO: Add a per cpu buffer only for reclaim threads
    if (opt_sync_reclaim == "normal"){
        ::memory::free_page(vaddr);
    } else {
        bool is_from_app = sched::thread::current() -> is_app();
        if (is_from_app){
            ::memory::free_page(vaddr);
        } else{
            ::memory::free_page_to_reclaim_buf(vaddr);
        }
    }
}

size_t page_list_t::get_active_list(size_t hint_list, bool slice, bool reclaim, bool *back){
    // It seems that we cannot have a general framework for whatever number of 
    // LRU queues because there is a very important policy decision where we 
    // slice and push pages
    // This is the first attempt where I simply use 2 queues and try to balance 
    // the length of these two. static is anyway not a good approach
    // The balancing is only for active. Not for clean. clean is static but it 
    // does not matter much. It is just a buffer anyway which should have a 
    // length near 0
    
    // Find the longest for slice, shortest for push. Only works for 2 queues
    // for the time being

    // The alternatives are:
    // 1. static (for example, cpu_id % max_queues, but this requires as many 
    // reclamation threads as max_queues)
    // 2. random (rand() % max_queues)
    // 3. Round Robin
    //      a. page fault static + reclaim RR slice (we can temporarily use 
    //      hard/soft to discriminate)
    //      b. page fault RR + reclaim RR
    // 4. size-based LB
    
    // NOTE: Does it work as well in the bootup phase?
    auto cpu_id = sched::cpu::current() -> id;
    if (opt_lru_mode == "static_lru"){
        hint_list = cpu_id % max_queues;
    } else if (opt_lru_mode == "static_fifo"){
        hint_list = cpu_id % max_queues;
        *back = (slice) ? false : true;
    } else if (opt_lru_mode == "rr"){
        // Still follow LRU
        rr_history[cpu_id] = (slice) ? (rr_history[cpu_id] + 1) % max_queues :\
            rr_history[cpu_id];
        hint_list = rr_history[cpu_id];
    } else if (opt_lru_mode == "rr_spec_async"){
        if (reclaim){
            // Async reclaim
            rr_history[cpu_id] = (slice) ? (rr_history[cpu_id] + 1) % max_queues :\
                rr_history[cpu_id];
            hint_list = rr_history[cpu_id];
        }else {
            // Sync reclaim
            hint_list = cpu_id % max_queues;
        }
    } else if (opt_lru_mode == "lb"){
        if (max_queues == 2){
            hint_list = slice ^ (sizes[0] >= sizes[1]);
        } else {
            if (slice){
                // longest
                size_t tmp = sizes[0];
                for (size_t i = 1; i < max_queues; i++){
                    hint_list = sizes[i] > tmp ? i : hint_list;
                    tmp = sizes[i] > tmp ? sizes[i] : tmp;
                }
            } else {
                // shortest
                size_t tmp = sizes[0];
                for (size_t i = 1; i < max_queues; i++){
                    hint_list = sizes[i] < tmp ? i : hint_list;
                    tmp = sizes[i] < tmp ? sizes[i] : tmp;
                }
            }
        }
    } else if (opt_lru_mode == "dynamic"){
        if (slice) {
            hint_list = reclaim ? (rr_history[cpu_id] + 1) : cpu_id;
            hint_list %= max_queues;
            while(!sizes[hint_list]) {hint_list = (hint_list + 1) % max_queues;}
            if (reclaim) rr_history[cpu_id] = hint_list;
        } else {
            // Push
            // Calculate throughput
            auto now = osv::clock::uptime::now();
            auto dur_nano = (now - *percpu_last_tp).count();
            uint64_t fault_in_tput = *percpu_last_tput;
            if (dur_nano >= 0 && dur_nano >= time_thres){
                auto new_fault_in = n_faultin.load(std::memory_order::memory_order_relaxed);
                auto diff_fault_in = new_fault_in - *percpu_fault_in_snapshot;
                fault_in_tput = diff_fault_in / (dur_nano / 10000);
                // Update 
                *percpu_fault_in_snapshot = new_fault_in;
                *percpu_last_tp = now;
                *percpu_last_tput = fault_in_tput;
            }
            // Decide how many lists
            auto used_queues =  fault_in_tput / tput_thres + 1;
            used_queues = (used_queues > max_queues) ? max_queues: used_queues;
            auto dur_log_nano = (now - *percpu_last_log).count();
            if (dur_log_nano >0 && dur_log_nano > log_thres){
                trace_ddc_dynamic_stat(dur_nano, fault_in_tput, used_queues);
                *percpu_last_log = now;
            }
            hint_list = cpu_id % used_queues;
            *back = (fault_in_tput > fifo_thres) ? false : true;
        }
    } else {
        abort("Not supported LRU");
    }
    
    // Handle empty list case for slicing
    while (slice && !sizes[hint_list]){
        hint_list = (hint_list + 1) % max_queues;
    }
    return hint_list;
}

size_t page_list_t::get_clean_list(size_t hint_list, bool slice, bool reclaim){
    // Only works for 2 Qs for now
    hint_list = hint_list / (ddc::max_cpu / max_queues + 1);
    assert(hint_list < max_queues);
    return hint_list;
}

void insert_page_buffered(base_page_t &page, bool try_flush) {
    (*active_page_buffer)->push_back(page);
    assert(page.ptep.read().addr() != NULL);
    if (try_flush) {
        try_flush_buffered();
    }
}

void try_flush_buffered() {
    if ((*active_page_buffer)->size() >= active_list_buffer_size) {
        base_page_slice_t temp_list;
        temp_list.splice(temp_list.end(), **active_page_buffer);
        DROP_LOCK(preempt_lock) { 
            bool back = true;
            size_t list_id = page_list.get_active_list(0, false, false, &back);
            page_list.push_pages_active(list_id, temp_list, true); 
        }
    }
}

}  // namespace ddc