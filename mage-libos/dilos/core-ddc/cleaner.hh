#pragma once

#include <ddc/memory.hh>
#include <ddc/mmu.hh>
#include <ddc/remote.hh>
#include <ddc/stat.hh>

#include "pte.hh"
#include "tlb.hh"

STAT_ELAPSED(ddc_accessed_handler_bottom_half);
STAT_ELAPSED(ddc_dirty_handler_wait);

TRACEPOINT(trace_ddc_poll_until_one_locked, "");
TRACEPOINT(trace_ddc_poll_until_one_locked_ret, "");
namespace ddc {
// cleaner should be thread-safe as well
class cleaner : public tlb_flusher {
   private:

   public:
    enum class state {
        OK_SKIP,
        OK_TLB_PUSHED,
        OK_PUSHED,
        OK_FINISH,
        PTE_UPDATE_FAIL,
    };

    cleaner(std::array<remote_queue, max_cpu> &percpu_rq, int qp_id, const size_t max_push)
        : _percpu_rq(percpu_rq),_percpu_pushed({0}), _percpu_mutex(), _qp_id(qp_id), _max_push(max_push) {
            std::fill(_percpu_pushed.begin(), _percpu_pushed.end(), 0);
            assert(percpu_rq.size() >= sched::cpus.size());
        }
    
    cleaner(const cleaner&) = delete;
    
    bool check_skip(mmu::pt_element<base_page_level> &pte) {
        if (is_protected(pte)) {
            // It's okay - fault handler will process this
            return true;
        } else if (is_remote(pte)) {
            // It's okay - already remote
            return true;
        }
        return false;
    }

    state process(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                  uintptr_t offset) {
        mmu::pt_element<base_page_level> old = ptep.read();
        if (check_skip(old)) {
            // It's okay - fault handler will process this
            return state::OK_SKIP;
        }
        if (old.valid()) {
            if (old.accessed()) {
                return accessed_handler(token, ptep, old, offset);
            }
            if (old.dirty()) {
                return dirty_handler(token, ptep, old, offset);
            } else {
                return clean_handler(token, ptep, old, offset);
            }
        }

        abort("noreach\n");
    }

    state process_mask(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                       uintptr_t offset, uintptr_t va, uintptr_t &vec) {
        mmu::pt_element<base_page_level> old = ptep.read();
        if (check_skip(old)) {
            // It's okay - fault handler will process this
            return state::OK_SKIP;
        }
        if (old.valid()) {
            if (old.accessed()) {
                return accessed_handler(token, ptep, old, offset);
            }
            if (old.dirty()) {
                return dirty_handler(token, ptep, old, offset, va, vec);
            } else {
                return clean_handler(token, ptep, old, offset, va, vec);
            }
        }

        abort("noreach\n");
    }

    state accessed_handler(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                           mmu::pt_element<base_page_level> old,
                           uintptr_t offset) {
        auto new_pte = old;
        new_pte.set_accessed(false);
        if (!ptep.compare_exchange(old, new_pte)) {
            return state::PTE_UPDATE_FAIL;
        }
        tlb_push(token);
        return state::OK_TLB_PUSHED;
    }
    
    state accessed_handler_up_half(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                           mmu::pt_element<base_page_level> old,
                           uintptr_t offset) {
        auto new_pte = old;
        new_pte.set_accessed(false);
        if (!ptep.compare_exchange(old, new_pte)) {
            return state::PTE_UPDATE_FAIL;
        }
        return state::OK_TLB_PUSHED;
    }

    void accessed_handler_bottom_half(uintptr_t token) {
        tlb_push(token);
    }

    state dirty_handler_inner(uintptr_t token,
                              mmu::pt_element<base_page_level> old,
                              uintptr_t offset, uintptr_t vec) {
        mmu::phys paddr = old.addr();
        assert(!sched::thread::current()->migratable());
        unsigned cpu_id = sched::cpu::current()->id;
        SCOPE_LOCK(_percpu_mutex[cpu_id]);
        auto &rq = _percpu_rq[cpu_id];
        auto &pushed = _percpu_pushed[cpu_id];
        while (pushed >= _max_push) {
            if (cpu_id < 54) trace_ddc_poll_until_one_locked();
            poll_until_one_locked(cpu_id);
            if (cpu_id < 54) trace_ddc_poll_until_one_locked_ret();
        }
        assert(pushed < _max_push);
        SCOPE_LOCK(preempt_lock);
        rq.push_vec(_qp_id, token, mmu::phys_to_virt(paddr), offset, vec);
        pushed ++;

        return state::OK_PUSHED;
    }
    
    state dirty_handler_inner(uintptr_t token,
                              mmu::pt_element<base_page_level> old,
                              uintptr_t offset) {
        mmu::phys paddr = old.addr();
        assert(!sched::thread::current()->migratable());
        unsigned cpu_id = sched::cpu::current()->id;
        SCOPE_LOCK(_percpu_mutex[cpu_id]);
        auto &rq = _percpu_rq[cpu_id];
        auto &pushed = _percpu_pushed[cpu_id];
        while ( pushed >= _max_push) {
            poll_until_one_locked(cpu_id);
        }
        assert(pushed < _max_push);
        SCOPE_LOCK(preempt_lock);
        rq.push(_qp_id, token, mmu::phys_to_virt(paddr), offset, base_page_size);
        pushed ++;

        return state::OK_PUSHED;
    }


    state dirty_handler(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                        mmu::pt_element<base_page_level> old,
                        uintptr_t offset) {
        // push
        auto new_pte = old;
        new_pte.set_dirty(false);
        if (!ptep.compare_exchange(old, new_pte)) {
            return state::PTE_UPDATE_FAIL;
        }
        mmu::phys paddr = old.addr();
        assert(!sched::thread::current()->migratable());
        unsigned cpu_id = sched::cpu::current()->id;
        SCOPE_LOCK(_percpu_mutex[cpu_id]);
        auto &rq = _percpu_rq[cpu_id];
        auto &pushed = _percpu_pushed[cpu_id];
        // Hard coded
        if (cpu_id < 54) trace_ddc_poll_until_one_locked();
        while (pushed >= _max_push) {
            poll_until_one_locked(cpu_id);
        }
        if (cpu_id < 54) trace_ddc_poll_until_one_locked_ret();
        assert(pushed < _max_push);
        SCOPE_LOCK(preempt_lock);
        rq.push(_qp_id, token, mmu::phys_to_virt(paddr), offset,
                 base_page_size);
        pushed ++;

        return state::OK_PUSHED;
    }

    state dirty_handler(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                        mmu::pt_element<base_page_level> old, uintptr_t offset,
                        uintptr_t va, uintptr_t &vec) {
        // push
        auto new_pte = old;
        new_pte.set_dirty(false);
        if (!ptep.compare_exchange(old, new_pte)) {
            return state::PTE_UPDATE_FAIL;
        }
        vec = get_vec(va);
        return dirty_handler_inner(token, old, offset, vec);
    }

    state dirty_handler_up_half(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                        mmu::pt_element<base_page_level> old, uintptr_t offset,
                        uintptr_t va, uintptr_t &vec) {
        // push
        auto new_pte = old;
        new_pte.set_dirty(false);
        if (!ptep.compare_exchange(old, new_pte)) {
            return state::PTE_UPDATE_FAIL;
        }
        vec = get_vec(va);
        return state::OK_PUSHED;
    }
    
    state dirty_handler_up_half(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                        mmu::pt_element<base_page_level> old,
                        uintptr_t offset) {
        // push
        auto new_pte = old;
        new_pte.set_dirty(false);
        if (!ptep.compare_exchange(old, new_pte)) {
            return state::PTE_UPDATE_FAIL;
        }
        return state::OK_PUSHED;
    }

    void dirty_handler_bottom_half(uintptr_t token,
                              mmu::pt_element<base_page_level> old,
                              uintptr_t offset, uintptr_t vec){
        dirty_handler_inner(token, old, offset, vec);
    }

    void dirty_handler_bottom_half(uintptr_t token,
                              mmu::pt_element<base_page_level> old,
                              uintptr_t offset){
        dirty_handler_inner(token, old, offset);
    }

    virtual uint64_t get_vec(uintptr_t va) {
        abort("unimplemented\n");
        return 0;
    }

    virtual void push_after(uintptr_t token) = 0;

    virtual state clean_handler(uintptr_t token,
                                mmu::hw_ptep<base_page_level> ptep,
                                mmu::pt_element<base_page_level> old,
                                uintptr_t offset) {
        abort("call unimpled clean handler");
    }

    virtual state clean_handler(uintptr_t token,
                                mmu::hw_ptep<base_page_level> ptep,
                                mmu::pt_element<base_page_level> old,
                                uintptr_t offset, uintptr_t va,
                                uintptr_t &vec) {
        abort("call unimpled clean (mask) handler");
    }

    inline bool cleaner_empty(unsigned cpu_id) { return _percpu_pushed[cpu_id] == 0; }
    // This function does not provide a strong guarantee that the percpu_rq's are all 
    // empty
    bool cleaner_all_empty(){
        bool is_empty = true;
        for (size_t i = 0; i < sched::cpus.size(); i++){
            SCOPE_LOCK(_percpu_mutex[i]);
            is_empty &= (_percpu_pushed[i] == 0);
        }
        return is_empty;
    }

    // Assume this function is called when migration disabled
    size_t poll_once(unsigned cpu_id) {
        assert(!sched::thread::current()->migratable());
        _percpu_mutex[cpu_id].lock();
        if (cleaner_empty(cpu_id)) {
            _percpu_mutex[cpu_id].unlock();
            return 0;
        }
        SCOPE_LOCK(preempt_lock);
        auto &rq = _percpu_rq[cpu_id];
        auto &pushed = _percpu_pushed[cpu_id];
        uintptr_t tokens[pushed];
        int polled = rq.poll(tokens, pushed);
        //printf("In poll once pushed: %u, polled: %u \n", pushed.load(), polled);
        assert(pushed != 224);
        pushed -= polled;
        assert(pushed >= 0);
        _percpu_mutex[cpu_id].unlock();
        for (int i = 0; i < polled; ++i) {
            push_after(tokens[i]);
        }
        return polled;
    }

    // Assume this function is called when migration disabled
    size_t poll_once_locked(unsigned cpu_id){
        assert(!sched::thread::current()->migratable());
        if (cleaner_empty(cpu_id)) return 0;
        SCOPE_LOCK(preempt_lock);
        auto &rq = _percpu_rq[cpu_id];
        auto &pushed = _percpu_pushed[cpu_id];
        uintptr_t tokens[pushed];
        int polled = rq.poll(tokens, pushed);
        //printf("In poll once locked pushed: %u, polled: %u \n", pushed.load(), polled);
        assert(pushed != 224);
        pushed -= polled;
        assert(pushed >= 0);
        for (int i = 0; i < polled; ++i) {
            push_after(tokens[i]);
        }
        return polled;

    }

    // Assume this function is called when migration disabled;
    size_t poll_until_one_locked(unsigned cpu_id){
        assert(!sched::thread::current()->migratable());
        assert(sched::preemptable());
        size_t polled = poll_once_locked(cpu_id);
        //printf("In poll until one locked pushed: %u, polled: %u \n", pushed.load(), polled);
        while ((_percpu_pushed[cpu_id] != 0 ) && !polled) {
            STAT_ELAPSED_FROM_WRAP(ddc_dirty_handler_wait)
            sched::cpu::schedule();
            STAT_ELAPSED_TO_WRAP(ddc_dirty_handler_wait)
            polled = poll_once_locked(cpu_id);
            //printf("In poll until one locked pushed: %u, polled: %u \n", pushed.load(), polled);
        }
        return polled;

    }

    // Assume this function is called when migration disabled;
    size_t poll_until_one(unsigned cpu_id) {
        assert(!sched::thread::current()->migratable());
        assert(sched::preemptable());
        size_t polled = poll_once(cpu_id);
        //printf("In poll until one pushed: %u, polled: %u \n", pushed.load(), polled);
        while ((_percpu_pushed[cpu_id] != 0 ) && !polled) {
            STAT_ELAPSED_FROM_WRAP(ddc_dirty_handler_wait)
            sched::cpu::schedule();
            STAT_ELAPSED_TO_WRAP(ddc_dirty_handler_wait)
            polled = poll_once(cpu_id);
            //printf("In poll until one pushed: %u, polled: %u \n", pushed.load(), polled);
        }
        return polled;
    }

    // Assume this function is called when migration disabled
    size_t poll_all(unsigned cpu_id) {
        assert(!sched::thread::current()->migratable());
        assert(sched::preemptable());
        size_t polled = poll_once(cpu_id);
        //printf("In poll all pushed: %u, polled: %u \n", pushed.load(), polled);
        while (_percpu_pushed[cpu_id] != 0) {
            sched::cpu::schedule();
            polled += poll_once(cpu_id);
            //printf("In poll all pushed: %u, polled: %u \n", pushed.load(), polled);
        }
        return polled;
    }

   public:
    // The problem here is that cleaner only has a reference so the remote queue 
    // can be used by some one else as well.
    // It seems ok because others just read stats from the queue
    // Anyway the remote_queue should be atomic. For the sake of time just put 
    // the lock here. 

    std::array<remote_queue, max_cpu> &_percpu_rq;
    std::array<size_t, max_cpu> _percpu_pushed;

    // Since calling schedule is now allowed in poll_all and poll_until_one
    // A l1/l2 refill threads can run during that time, which can modify the 
    // percpu_rq as well
    // So we either remove schedule and add preempt_lock
    // Or add percpu mutex
    std::array<mutex, max_cpu> _percpu_mutex;
        
    int _qp_id;
    const size_t _max_push;
};

// This is the class used for madvise which owns its resources
class cleaner_madv : public tlb_flusher_madv {
   private:

   public:
    enum class state {
        OK_SKIP,
        OK_TLB_PUSHED,
        OK_PUSHED,
        OK_FINISH,
        PTE_UPDATE_FAIL,
    };

    cleaner_madv(std::array<remote_queue, max_cpu> &percpu_rq, int qp_id, const size_t max_push)
        : _percpu_rq(percpu_rq),_percpu_pushed({0}), _percpu_mutex(), _qp_id(qp_id), _max_push(max_push) {
            std::fill(_percpu_pushed.begin(), _percpu_pushed.end(), 0);
            assert(percpu_rq.size() >= sched::cpus.size());
        }
    
    cleaner_madv(const cleaner_madv&) = delete;
    
    bool check_skip(mmu::pt_element<base_page_level> &pte) {
        if (is_protected(pte)) {
            // It's okay - fault handler will process this
            return true;
        } else if (is_remote(pte)) {
            // It's okay - already remote
            return true;
        }
        return false;
    }

    state process(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                  uintptr_t offset) {
        mmu::pt_element<base_page_level> old = ptep.read();
        if (check_skip(old)) {
            // It's okay - fault handler will process this
            return state::OK_SKIP;
        }
        if (old.valid()) {
            if (old.accessed()) {
                return accessed_handler(token, ptep, old, offset);
            }
            if (old.dirty()) {
                return dirty_handler(token, ptep, old, offset);
            } else {
                return clean_handler(token, ptep, old, offset);
            }
        }

        abort("noreach\n");
    }

    state process_mask(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                       uintptr_t offset, uintptr_t va, uintptr_t &vec) {
        mmu::pt_element<base_page_level> old = ptep.read();
        if (check_skip(old)) {
            // It's okay - fault handler will process this
            return state::OK_SKIP;
        }
        if (old.valid()) {
            if (old.accessed()) {
                return accessed_handler(token, ptep, old, offset);
            }
            if (old.dirty()) {
                return dirty_handler(token, ptep, old, offset, va, vec);
            } else {
                return clean_handler(token, ptep, old, offset, va, vec);
            }
        }

        abort("noreach\n");
    }

    state accessed_handler(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                           mmu::pt_element<base_page_level> old,
                           uintptr_t offset) {
        auto new_pte = old;
        new_pte.set_accessed(false);
        if (!ptep.compare_exchange(old, new_pte)) {
            return state::PTE_UPDATE_FAIL;
        }
        tlb_push(token);
        return state::OK_TLB_PUSHED;
    }

    state dirty_handler_inner(uintptr_t token,
                              mmu::pt_element<base_page_level> old,
                              uintptr_t offset, uintptr_t vec) {
        mmu::phys paddr = old.addr();
        assert(!sched::thread::current()->migratable());
        unsigned cpu_id = sched::cpu::current()->id;
        SCOPE_LOCK(_percpu_mutex[cpu_id]);
        auto &rq = _percpu_rq[cpu_id];
        auto &pushed = _percpu_pushed[cpu_id];
        while (pushed >= _max_push) {
            poll_until_one_locked(cpu_id);
        }
        assert(pushed < _max_push);
        SCOPE_LOCK(preempt_lock);
        rq.push_vec(_qp_id, token, mmu::phys_to_virt(paddr), offset, vec);
        pushed ++;

        return state::OK_PUSHED;
    }
    
    state dirty_handler_inner(uintptr_t token,
                              mmu::pt_element<base_page_level> old,
                              uintptr_t offset) {
        mmu::phys paddr = old.addr();
        assert(!sched::thread::current()->migratable());
        unsigned cpu_id = sched::cpu::current()->id;
        SCOPE_LOCK(_percpu_mutex[cpu_id]);
        auto &rq = _percpu_rq[cpu_id];
        auto &pushed = _percpu_pushed[cpu_id];
        while ( pushed >= _max_push) {
            poll_until_one_locked(cpu_id);
        }
        assert(pushed < _max_push);
        SCOPE_LOCK(preempt_lock);
        rq.push(_qp_id, token, mmu::phys_to_virt(paddr), offset, base_page_size);
        pushed ++;

        return state::OK_PUSHED;
    }


    state dirty_handler(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                        mmu::pt_element<base_page_level> old,
                        uintptr_t offset) {
        // push
        auto new_pte = old;
        new_pte.set_dirty(false);
        if (!ptep.compare_exchange(old, new_pte)) {
            return state::PTE_UPDATE_FAIL;
        }
        mmu::phys paddr = old.addr();
        assert(!sched::thread::current()->migratable());
        unsigned cpu_id = sched::cpu::current()->id;
        SCOPE_LOCK(_percpu_mutex[cpu_id]);
        auto &rq = _percpu_rq[cpu_id];
        auto &pushed = _percpu_pushed[cpu_id];
        // Hard coded
        while (pushed >= _max_push) {
            poll_until_one_locked(cpu_id);
        }
        assert(pushed < _max_push);
        SCOPE_LOCK(preempt_lock);
        rq.push(_qp_id, token, mmu::phys_to_virt(paddr), offset,
                 base_page_size);
        pushed ++;

        return state::OK_PUSHED;
    }

    state dirty_handler(uintptr_t token, mmu::hw_ptep<base_page_level> ptep,
                        mmu::pt_element<base_page_level> old, uintptr_t offset,
                        uintptr_t va, uintptr_t &vec) {
        // push
        auto new_pte = old;
        new_pte.set_dirty(false);
        if (!ptep.compare_exchange(old, new_pte)) {
            return state::PTE_UPDATE_FAIL;
        }
        vec = get_vec(va);
        return dirty_handler_inner(token, old, offset, vec);
    }

    virtual uint64_t get_vec(uintptr_t va) {
        abort("unimplemented\n");
        return 0;
    }

    virtual void push_after(uintptr_t token) = 0;

    virtual state clean_handler(uintptr_t token,
                                mmu::hw_ptep<base_page_level> ptep,
                                mmu::pt_element<base_page_level> old,
                                uintptr_t offset) {
        abort("call unimpled clean handler");
    }

    virtual state clean_handler(uintptr_t token,
                                mmu::hw_ptep<base_page_level> ptep,
                                mmu::pt_element<base_page_level> old,
                                uintptr_t offset, uintptr_t va,
                                uintptr_t &vec) {
        abort("call unimpled clean (mask) handler");
    }

    inline bool cleaner_empty(unsigned cpu_id) { return _percpu_pushed[cpu_id] == 0; }
    // This function does not provide a strong guarantee that the percpu_rq's are all 
    // empty
    bool cleaner_all_empty(){
        bool is_empty = true;
        for (size_t i = 0; i < sched::cpus.size(); i++){
            SCOPE_LOCK(_percpu_mutex[i]);
            is_empty &= (_percpu_pushed[i] == 0);
        }
        return is_empty;
    }

    // Assume this function is called when migration disabled
    size_t poll_once(unsigned cpu_id) {
        assert(!sched::thread::current()->migratable());
        _percpu_mutex[cpu_id].lock();
        if (cleaner_empty(cpu_id)) {
            _percpu_mutex[cpu_id].unlock();
            return 0;
        }
        SCOPE_LOCK(preempt_lock);
        auto &rq = _percpu_rq[cpu_id];
        auto &pushed = _percpu_pushed[cpu_id];
        uintptr_t tokens[pushed];
        int polled = rq.poll(tokens, pushed);
        //printf("In poll once pushed: %u, polled: %u \n", pushed.load(), polled);
        assert(pushed != 224);
        pushed -= polled;
        assert(pushed >= 0);
        _percpu_mutex[cpu_id].unlock();
        for (int i = 0; i < polled; ++i) {
            push_after(tokens[i]);
        }
        return polled;
    }

    // Assume this function is called when migration disabled
    size_t poll_once_locked(unsigned cpu_id){
        assert(!sched::thread::current()->migratable());
        if (cleaner_empty(cpu_id)) return 0;
        SCOPE_LOCK(preempt_lock);
        auto &rq = _percpu_rq[cpu_id];
        auto &pushed = _percpu_pushed[cpu_id];
        uintptr_t tokens[pushed];
        int polled = rq.poll(tokens, pushed);
        //printf("In poll once locked pushed: %u, polled: %u \n", pushed.load(), polled);
        assert(pushed != 224);
        pushed -= polled;
        assert(pushed >= 0);
        for (int i = 0; i < polled; ++i) {
            push_after(tokens[i]);
        }
        return polled;

    }

    // Assume this function is called when migration disabled;
    size_t poll_until_one_locked(unsigned cpu_id){
        assert(!sched::thread::current()->migratable());
        assert(sched::preemptable());
        size_t polled = poll_once_locked(cpu_id);
        //printf("In poll until one locked pushed: %u, polled: %u \n", pushed.load(), polled);
        while ((_percpu_pushed[cpu_id] != 0 ) && !polled) {
            STAT_ELAPSED_FROM_WRAP(ddc_dirty_handler_wait)
            sched::cpu::schedule();
            STAT_ELAPSED_TO_WRAP(ddc_dirty_handler_wait)
            polled = poll_once_locked(cpu_id);
            //printf("In poll until one locked pushed: %u, polled: %u \n", pushed.load(), polled);
        }
        return polled;

    }

    // Assume this function is called when migration disabled;
    size_t poll_until_one(unsigned cpu_id) {
        assert(!sched::thread::current()->migratable());
        assert(sched::preemptable());
        size_t polled = poll_once(cpu_id);
        //printf("In poll until one pushed: %u, polled: %u \n", pushed.load(), polled);
        while ((_percpu_pushed[cpu_id] != 0 ) && !polled) {
            STAT_ELAPSED_FROM_WRAP(ddc_dirty_handler_wait)
            sched::cpu::schedule();
            STAT_ELAPSED_TO_WRAP(ddc_dirty_handler_wait)
            polled = poll_once(cpu_id);
            //printf("In poll until one pushed: %u, polled: %u \n", pushed.load(), polled);
        }
        return polled;
    }

    // Assume this function is called when migration disabled
    size_t poll_all(unsigned cpu_id) {
        assert(!sched::thread::current()->migratable());
        assert(sched::preemptable());
        size_t polled = poll_once(cpu_id);
        //printf("In poll all pushed: %u, polled: %u \n", pushed.load(), polled);
        while (_percpu_pushed[cpu_id] != 0) {
            sched::cpu::schedule();
            polled += poll_once(cpu_id);
            //printf("In poll all pushed: %u, polled: %u \n", pushed.load(), polled);
        }
        return polled;
    }

   private:
    // The problem here is that cleaner only has a reference so the remote queue 
    // can be used by some one else as well.
    // It seems ok because others just read stats from the queue
    // Anyway the remote_queue should be atomic. For the sake of time just put 
    // the lock here. 

    std::array<remote_queue, max_cpu> &_percpu_rq;
    std::array<size_t, max_cpu> _percpu_pushed;

    // Since calling schedule is now allowed in poll_all and poll_until_one
    // A l1/l2 refill threads can run during that time, which can modify the 
    // percpu_rq as well
    // So we either remove schedule and add preempt_lock
    // Or add percpu mutex
    std::array<mutex, max_cpu> _percpu_mutex;
        
    int _qp_id;
    const size_t _max_push;
};

}  // namespace ddc