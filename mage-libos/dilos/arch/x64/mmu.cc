/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "arch-cpu.hh"
#include <osv/debug.hh>
#include <osv/sched.hh>
#include <osv/mmu.hh>
#include <osv/irqlock.hh>
#include <osv/interrupt.hh>
#include <osv/migration-lock.hh>
#include <osv/prio.hh>
#include <osv/elf.hh>
#include "exceptions.hh"
#include <lockfree/queue-mpsc.hh>
#include <osv/spinlock.h>
#include <osv/percpu.hh>

#include <ddc/stat.hh>
TRACEPOINT(trace_ddc_async_tlb_debug, "");

STAT_ELAPSED(flush_tlb_all_inside_lock);
STAT_ELAPSED(flush_tlb_all_core_inside_lock);
STAT_ELAPSED(flush_tlb_all_core_wait_send_inside_lock);
STAT_ELAPSED(flush_tlb_some_core_inside_lock);
STAT_ELAPSED(flush_tlb_some_core_send_ipi);
STAT_ELAPSED(flush_tlb_all_wait_inside_lock);
STAT_COUNTER(flush_tlb_check);

PERCPU(uint64_t, tlb_local_flush);
PERCPU(uint64_t, tlb_check_cpus);
PERCPU(uint64_t, tlb_send_ipi);
PERCPU(uint64_t, tlb_wait_ipi);
void page_fault(exception_frame *ef)
{
    auto start = processor::ticks();
    sched::fpu_lock fpu;
    SCOPE_LOCK(fpu);
    sched::exception_guard g;
    auto addr = processor::read_cr2();
    if (fixup_fault(ef)) {
        return;
    }
    auto pc = reinterpret_cast<void*>(ef->rip);
    if (!pc) {
        abort("trying to execute null pointer");
    }
    if (reinterpret_cast<void*>(addr & ~(mmu::page_size - 1)) == elf::missing_symbols_page_addr) {
        abort("trying to execute or access missing symbol");
    }
    // The following code may sleep. So let's verify the fault did not happen
    // when preemption was disabled, or interrupts were disabled.
    assert(sched::preemptable());
    assert(ef->rflags & processor::rflags_if);

    // And since we may sleep, make sure interrupts are enabled.
    DROP_LOCK(irq_lock) { // irq_lock is acquired by HW
        mmu::vm_fault(addr, ef);
    }
    auto end = processor::ticks();
    (*_percpu_vmfault_cycles) += end - start;
}

namespace mmu {

uint8_t phys_bits = max_phys_bits, virt_bits = 52;

void flush_tlb_local() {
    // TODO: we can use page_table_root instead of read_cr3(), can be faster
    // when shadow page tables are used.
    processor::write_cr3(processor::read_cr3());
}

// tlb_flush() does TLB flush on *all* processors, not returning before all
// processors confirm flushing their TLB. This is slow, but necessary for
// correctness so that, for example, after mprotect() returns, no thread on
// no cpu can write to the protected page.

// We will change this to percpu variables
// This should be replaced by cpu_mask
// TODO: Add cpumask support
std::array<std::array<sched::cpu*, ddc::max_cpu>, ddc::max_cpu> tlb_flush_ipis;

// TODO: These need to be percpu variables
std::array<std::atomic<uint64_t>, ddc::max_cpu> tlb_flush_pendingconfirms;

std::array<std::atomic<uint64_t>, ddc::max_cpu> tlb_flush_pendingconfirms_cooperative;
struct cfd {
    // TODO: Replace this with a percpu array
    std::array<lockfree::linked_item<uint64_t>, ddc::max_cpu> csd;
};
std::array<struct cfd, ddc::max_cpu> tlb_flush_cfd;

std::array<struct cfd, ddc::max_cpu> tlb_flush_cfd_cooperative;

class test_queue {
public:
    uint64_t depth = 0;
    lockfree::linked_item<uint64_t>* head = nullptr;
    lockfree::linked_item<uint64_t>* tail = nullptr;
lockfree::linked_item<uint64_t>* pop(){
    if (!head) {
        assert(!tail);
        return nullptr;
    }
    lockfree::linked_item<uint64_t>* temp =head;
    head = head->next;
    if (!head) tail = nullptr;
    depth -=1;
    return temp;
}
void push(lockfree::linked_item<uint64_t>* node){
    if (!head) {
        assert(!tail);
        head = tail = node;
    } else {
        tail->next = node;
        tail = node;
    }
    depth +=1;
}
};

std::array<lockfree::queue_mpsc<lockfree::linked_item<uint64_t>>, ddc::max_cpu> tlb_flush_queue;

spinlock_t debug_lock;

inter_processor_interrupt tlb_flush_ipi{IPI_TLB_FLUSH, [] {
        assert(!arch::irq_enabled());
        uint64_t current_cpu = sched::cpu::current()->id;
        // Ensure we pop everything from the item if we can, because if interrupts were disabled on this core
        // the number of IPIs waiting to be posted may not be same as number of elements pushed into the queue.
        // In such coalescing, we need to adopt a different approach where we remove and consume as many elements
        // as possible. The other IPIs will then just come in and return without doing anything.
        mmu::flush_tlb_local();
        flush_tlb_update_queue(current_cpu);
        return;
}};

void flush_tlb_cooperative() {
    assert(!sched::preemptable());
    //assert(sched::thread::current()->is_app());
    if (sched::cpus.size() <= 1){
        mmu::flush_tlb_local();
        return;
    }
    mmu::flush_tlb_local();
    uint64_t count = 0;

    // The sched class only supports less than 64 CPUs because it uses atomic unsigned long
    // as the cpu mask
    uint64_t cpu_mask = 0;
    auto cpu_id = sched::cpu::current()->id;
    flush_tlb_update_queue(cpu_id);
    // TODO: This needs to be eliminated in the initialization of percpu variable

    for (uint64_t ii = 0; ii < ddc::max_cpu; ii++){
        tlb_flush_cfd_cooperative[cpu_id].csd[ii].value =cpu_id | tlb_from_cooperative;
    }

    for (auto core = sched::cpus.begin(); core != sched::cpus.end(); core ++){
        if ((*core) == sched::cpu::current()) {
            continue;
        }
        (*core) -> lazy_flush_tlb.store(true, std::memory_order_relaxed);
        if (!(*core)->app_thread.load(std::memory_order_seq_cst)) {
            continue;
        }
        if (!(*core)->lazy_flush_tlb.exchange(false, std::memory_order_relaxed)) {
            continue;
        }
        tlb_flush_ipis[cpu_id][count++] = (*core);
        cpu_mask |= (1ul << ((*core)->id));
    }
    if (count == 0)
        return;
    // Safe to store the count directly
    uint64_t tmp = 0;
    // WITH_LOCK(debug_lock){
    //     debug_ll("co %u %lu\n", cpu_id, tlb_flush_pendingconfirms_cooperative[cpu_id].load());
    // }
    assert(tlb_flush_pendingconfirms_cooperative[cpu_id].compare_exchange_strong(tmp, cpu_mask) == true);
    if (count + 1 == sched::cpus.size()) {
        // Set all the tlb_flush_from_cpu to be my cpu_id
        for (uint64_t core = 0; core != sched::cpus.size(); core ++){
            if(core == cpu_id) continue;
            assert(tlb_flush_cfd_cooperative[cpu_id].csd[core].next == nullptr);
            tlb_flush_queue[core].push(&tlb_flush_cfd_cooperative[cpu_id].csd[core]);
            // Set Cacheline to 1
            mmu::tlb_flush_cachelines[core].cacheline[0] = 1;
        }
    } else {
        // Set all the tlb_flush_from_cpu in ipis to be my cpu_id
        for (uint64_t iter = 0; iter < count; iter ++) {
            auto &c = tlb_flush_ipis[cpu_id][iter];
            assert(c->id != cpu_id);
            assert(tlb_flush_cfd_cooperative[cpu_id].csd[c->id].next == nullptr);
            tlb_flush_queue[c->id].push(&tlb_flush_cfd_cooperative[cpu_id].csd[c->id]);
            // Set Cacheline to 1
            mmu::tlb_flush_cachelines[c->id].cacheline[0] = 1;
        }
    }
}

int flush_tlb_need_flush_check(unsigned current_cpu){
    return !tlb_flush_queue[current_cpu].empty();
}

void flush_tlb_check_and_flush(unsigned current_cpu){
    if (flush_tlb_need_flush_check(current_cpu)) {
        STAT_COUNTING(flush_tlb_check);
        mmu::flush_tlb_local();   
        flush_tlb_update_queue(current_cpu);
    }
}

/* 1 done, 0 undone */
int flush_tlb_cooperative_check(){
    assert(!sched::preemptable());
    if (sched::cpus.size() <= 1){
        return 1;
    }
    auto cpu_id = sched::cpu::current() -> id;
    return tlb_flush_pendingconfirms_cooperative[cpu_id].load() == 0;
}

void flush_tlb_update_queue(unsigned current_cpu){
    irq_save_lock_type irq_lock;
    SCOPE_LOCK(irq_lock);
    while (auto *node = tlb_flush_queue[current_cpu].pop()) {
        assert(node != nullptr);
        uint64_t from_cpu = node->value & (~tlb_from_cooperative);
        uint64_t from_cooperative = node->value & (tlb_from_cooperative);
        node->next = nullptr;
        assert(from_cpu != current_cpu);
        uint64_t tmp = (from_cooperative != 0) ? tlb_flush_pendingconfirms_cooperative[from_cpu].fetch_xor(1ul << current_cpu) 
            : tlb_flush_pendingconfirms[from_cpu].fetch_add(-1);
        assert(tmp >= 1);
    }
    // Set Cacheline on the current CPU 0
    mmu::tlb_flush_cachelines[current_cpu].cacheline[0] = 0;
}

// A kernel thread is flushing on behalf of the app thread
// We don't do local TLB flush since we are not application threads
void flush_tlb_on_behalf(unsigned request_cpu)
{
    if (sched::cpus.size() <= 1) {
        mmu::flush_tlb_local();
        return;
    }

    // Also add preempt_lock?
    SCOPE_LOCK(migration_lock);
    SCOPE_LOCK(preempt_lock);
    STAT_ELAPSED_FROM_WRAP(flush_tlb_all_inside_lock);
    mmu::flush_tlb_local();
    uint64_t count = 0;
    auto cpu_id = sched::cpu::current()->id;
    flush_tlb_update_queue(cpu_id);
    
    // TODO: This needs to be eliminated in the initialization of percpu variable
    for (uint64_t ii = 0; ii < ddc::max_cpu; ii++){
        tlb_flush_cfd[cpu_id].csd[ii].value =cpu_id;
    }

    // We know we are doing this on behalf of an app thread
    count = 0;
    uint64_t cpu_mask = tlb_flush_pendingconfirms_cooperative[request_cpu].load();
    for (auto core = sched::cpus.begin(); core != sched::cpus.end(); core ++){
        if ((*core) == sched::cpu::current()) {
            continue;
        }

        // Have to reap all the pending cooperative flush as well
        if (cpu_mask & (1ul << ((*core)->id))){
            tlb_flush_ipis[cpu_id][count++] = (*core);
            continue;
        }

        (*core) -> lazy_flush_tlb.store(true, std::memory_order_relaxed);
        if (!(*core)->app_thread.load(std::memory_order_seq_cst)) {
            continue;
        }
        if (!(*core)->lazy_flush_tlb.exchange(false, std::memory_order_relaxed)) {
            continue;
        }
        tlb_flush_ipis[cpu_id][count++] = (*core);
    }
    if (count == 0)
        return;
    // Safe to store the count directly
    uint64_t tmp = 0;

    // WITH_LOCK(debug_lock){
    //     debug_ll("al %u %lu\n", cpu_id, tlb_flush_pendingconfirms_cooperative[cpu_id].load());
    // }
    assert(tlb_flush_pendingconfirms[cpu_id].compare_exchange_strong(tmp, count) == true);
    if (count + 1 == sched::cpus.size()) {
#ifndef NO_STAT
        auto token_flush_all = STAT_ELAPSED_FROM();
#endif
        // Set all the tlb_flush_from_cpu to be my cpu_id
        for (uint64_t core = 0; core != sched::cpus.size(); core ++){
            if(core == cpu_id) continue;
            assert(tlb_flush_cfd[cpu_id].csd[core].next == nullptr);
            tlb_flush_queue[core].push(&tlb_flush_cfd[cpu_id].csd[core]);
            // Set Cacheline to 1
            mmu::tlb_flush_cachelines[core].cacheline[0] = 1;
        }
        STAT_ELAPSED_TO(flush_tlb_all_core_wait_send_inside_lock, token_flush_all);
        tlb_flush_ipi.send_allbutself();
        STAT_ELAPSED_TO(flush_tlb_all_core_inside_lock, token_flush_all);
    } else {
        STAT_ELAPSED_FROM_WRAP(flush_tlb_some_core_inside_lock);
        // Set all the tlb_flush_from_cpu in ipis to be my cpu_id
        for (uint64_t iter = 0; iter < count; iter ++) {
            auto &c = tlb_flush_ipis[cpu_id][iter];
            assert(c->id != cpu_id);
            assert(tlb_flush_cfd[cpu_id].csd[c->id].next == nullptr);
            tlb_flush_queue[c->id].push(&tlb_flush_cfd[cpu_id].csd[c->id]);
            mmu::tlb_flush_cachelines[c->id].cacheline[0] = 1;
            // Set Cacheline to 1
            tlb_flush_ipi.send(c);
        }
        STAT_ELAPSED_TO_WRAP(flush_tlb_some_core_inside_lock);
    }
    STAT_ELAPSED_FROM_WRAP(flush_tlb_all_wait_inside_lock);
    while (tlb_flush_pendingconfirms[cpu_id].load() != 0);
    STAT_ELAPSED_TO_WRAP(flush_tlb_all_wait_inside_lock);
    STAT_ELAPSED_TO_WRAP(flush_tlb_all_inside_lock);
    assert(tlb_flush_pendingconfirms_cooperative[request_cpu].load() == 0);
}

void flush_tlb_all_send_pipelined(unsigned cpu_id){
    auto tick1 = processor::ticks();
    if (sched::cpus.size() <= 1) {
        mmu::flush_tlb_local();
        return;
    }

    // Also add preempt_lock?
    SCOPE_LOCK(migration_lock);
    SCOPE_LOCK(preempt_lock);
    mmu::flush_tlb_local();
    uint64_t count = 0;

    // Ack the pending ipi based flush or cacheline flush
    flush_tlb_update_queue(cpu_id);
    
    // TODO: This needs to be eliminated in the initialization of percpu variable
    for (uint64_t ii = 0; ii < ddc::max_cpu; ii++){
        tlb_flush_cfd[cpu_id].csd[ii].value =cpu_id;
    }
    auto tick2 = processor::ticks();
    if (sched::thread::current()->is_app()) {
        count = 0;
        uint64_t cpu_mask = tlb_flush_pendingconfirms_cooperative[cpu_id].load();
        for (auto core = sched::cpus.begin(); core != sched::cpus.end(); core ++){
            if ((*core) == sched::cpu::current()) {
                continue;
            }

            // Have to reap all the pending cooperative flush as well
            if (cpu_mask & (1ul << ((*core)->id))){
                tlb_flush_ipis[cpu_id][count++] = (*core);
                continue;
            }

            (*core) -> lazy_flush_tlb.store(true, std::memory_order_relaxed);
            if (!(*core)->app_thread.load(std::memory_order_seq_cst)) {
                continue;
            }
            if (!(*core)->lazy_flush_tlb.exchange(false, std::memory_order_relaxed)) {
                continue;
            }
            tlb_flush_ipis[cpu_id][count++] = (*core);
        }
    } else {
        count = sched::cpus.size() - 1;
    }
    if (count == 0)
        return;
    auto tick3 = processor::ticks();
    // Safe to store the count directly
    uint64_t tmp = 0;

    // WITH_LOCK(debug_lock){
    //     debug_ll("al %u %lu\n", cpu_id, tlb_flush_pendingconfirms_cooperative[cpu_id].load());
    // }
    assert(tlb_flush_pendingconfirms[cpu_id].compare_exchange_strong(tmp, count) == true);
    if (count + 1 == sched::cpus.size()) {
#ifndef NO_STAT
        auto token_flush_all = STAT_ELAPSED_FROM();
#endif
        // Set all the tlb_flush_from_cpu to be my cpu_id
        for (uint64_t core = 0; core != sched::cpus.size(); core ++){
            if(core == cpu_id) continue;
            assert(tlb_flush_cfd[cpu_id].csd[core].next == nullptr);
            tlb_flush_queue[core].push(&tlb_flush_cfd[cpu_id].csd[core]);
            mmu::tlb_flush_cachelines[core].cacheline[0] = 1;
            // Set Cacheline to 1
        }
        STAT_ELAPSED_TO(flush_tlb_all_core_wait_send_inside_lock, token_flush_all);
        tlb_flush_ipi.send_allbutself();
        STAT_ELAPSED_TO(flush_tlb_all_core_inside_lock, token_flush_all);
    } else {
        STAT_ELAPSED_FROM_WRAP(flush_tlb_some_core_inside_lock);
        // Set all the tlb_flush_from_cpu in ipis to be my cpu_id
        for (uint64_t iter = 0; iter < count; iter ++) {
            auto &c = tlb_flush_ipis[cpu_id][iter];
            assert(c->id != cpu_id);
            assert(tlb_flush_cfd[cpu_id].csd[c->id].next == nullptr);
            tlb_flush_queue[c->id].push(&tlb_flush_cfd[cpu_id].csd[c->id]);
            mmu::tlb_flush_cachelines[c->id].cacheline[0] = 1;
            // Set Cacheline to 1
            STAT_ELAPSED_FROM_WRAP(flush_tlb_some_core_send_ipi);
            tlb_flush_ipi.send(c);
            STAT_ELAPSED_TO_WRAP(flush_tlb_some_core_send_ipi);
        }
        STAT_ELAPSED_TO_WRAP(flush_tlb_some_core_inside_lock);
    }
    auto tick4 = processor::ticks();
    *tlb_local_flush += (tick2 - tick1);
    *tlb_check_cpus += (tick3 - tick2);
    *tlb_send_ipi += (tick4 - tick3);

}

void flush_tlb_all_ack_pipelined(unsigned cpu_id){
    auto tick = processor::ticks();
    SCOPE_LOCK(migration_lock);
    STAT_ELAPSED_FROM_WRAP(flush_tlb_all_wait_inside_lock);
    while (tlb_flush_pendingconfirms[cpu_id].load() != 0);
    assert(tlb_flush_pendingconfirms_cooperative[cpu_id].load() == 0);
    STAT_ELAPSED_TO_WRAP(flush_tlb_all_wait_inside_lock);

    *tlb_wait_ipi += (processor::ticks() - tick);
}

void flush_tlb_all()
{
    auto tick1 = processor::ticks();
    if (sched::cpus.size() <= 1) {
        mmu::flush_tlb_local();
        return;
    }

    auto cpu_id = sched::cpu::current()->id;

    // Also add preempt_lock?
    SCOPE_LOCK(migration_lock);
    SCOPE_LOCK(preempt_lock);
    STAT_ELAPSED_FROM_WRAP(flush_tlb_all_inside_lock);
    mmu::flush_tlb_local();
    uint64_t count = 0;

    // Ack the pending ipi based flush or cacheline flush
    flush_tlb_update_queue(cpu_id);
    
    // TODO: This needs to be eliminated in the initialization of percpu variable
    for (uint64_t ii = 0; ii < ddc::max_cpu; ii++){
        tlb_flush_cfd[cpu_id].csd[ii].value =cpu_id;
    }
    auto tick2 = processor::ticks();
    if (sched::thread::current()->is_app()) {
        count = 0;
        uint64_t cpu_mask = tlb_flush_pendingconfirms_cooperative[cpu_id].load();
        for (auto core = sched::cpus.begin(); core != sched::cpus.end(); core ++){
            if ((*core) == sched::cpu::current()) {
                continue;
            }

            // Have to reap all the pending cooperative flush as well
            if (cpu_mask & (1ul << ((*core)->id))){
                tlb_flush_ipis[cpu_id][count++] = (*core);
                continue;
            }

            (*core) -> lazy_flush_tlb.store(true, std::memory_order_relaxed);
            if (!(*core)->app_thread.load(std::memory_order_seq_cst)) {
                continue;
            }
            if (!(*core)->lazy_flush_tlb.exchange(false, std::memory_order_relaxed)) {
                continue;
            }
            tlb_flush_ipis[cpu_id][count++] = (*core);
        }
    } else {
        count = sched::cpus.size() - 1;
    }
    if (count == 0)
        return;
    auto tick3 = processor::ticks();
    // Safe to store the count directly
    uint64_t tmp = 0;

    // WITH_LOCK(debug_lock){
    //     debug_ll("al %u %lu\n", cpu_id, tlb_flush_pendingconfirms_cooperative[cpu_id].load());
    // }
    assert(tlb_flush_pendingconfirms[cpu_id].compare_exchange_strong(tmp, count) == true);
    if (count + 1 == sched::cpus.size()) {
#ifndef NO_STAT
        auto token_flush_all = STAT_ELAPSED_FROM();
#endif
        // Set all the tlb_flush_from_cpu to be my cpu_id
        for (uint64_t core = 0; core != sched::cpus.size(); core ++){
            if(core == cpu_id) continue;
            assert(tlb_flush_cfd[cpu_id].csd[core].next == nullptr);
            tlb_flush_queue[core].push(&tlb_flush_cfd[cpu_id].csd[core]);
            mmu::tlb_flush_cachelines[core].cacheline[0] = 1;
            // Set Cacheline to 1
        }
        STAT_ELAPSED_TO(flush_tlb_all_core_wait_send_inside_lock, token_flush_all);
        tlb_flush_ipi.send_allbutself();
        STAT_ELAPSED_TO(flush_tlb_all_core_inside_lock, token_flush_all);
    } else {
        STAT_ELAPSED_FROM_WRAP(flush_tlb_some_core_inside_lock);
        // Set all the tlb_flush_from_cpu in ipis to be my cpu_id
        for (uint64_t iter = 0; iter < count; iter ++) {
            auto &c = tlb_flush_ipis[cpu_id][iter];
            assert(c->id != cpu_id);
            assert(tlb_flush_cfd[cpu_id].csd[c->id].next == nullptr);
            tlb_flush_queue[c->id].push(&tlb_flush_cfd[cpu_id].csd[c->id]);
            mmu::tlb_flush_cachelines[c->id].cacheline[0] = 1;
            // Set Cacheline to 1
            STAT_ELAPSED_FROM_WRAP(flush_tlb_some_core_send_ipi);
            tlb_flush_ipi.send(c);
            STAT_ELAPSED_TO_WRAP(flush_tlb_some_core_send_ipi);
        }
        STAT_ELAPSED_TO_WRAP(flush_tlb_some_core_inside_lock);
    }
    auto tick4 = processor::ticks();
    STAT_ELAPSED_FROM_WRAP(flush_tlb_all_wait_inside_lock);
    while (tlb_flush_pendingconfirms[cpu_id].load() != 0);
    assert(tlb_flush_pendingconfirms_cooperative[cpu_id].load() == 0);
    STAT_ELAPSED_TO_WRAP(flush_tlb_all_wait_inside_lock);
    STAT_ELAPSED_TO_WRAP(flush_tlb_all_inside_lock);

    *tlb_local_flush += (tick2 - tick1);
    *tlb_check_cpus += (tick3 - tick2);
    *tlb_send_ipi += (tick4 - tick3);
    *tlb_wait_ipi += (processor::ticks() - tick4);
}

static pt_element<4> page_table_root __attribute__((init_priority((int)init_prio::pt_root)));

pt_element<4> *get_root_pt(uintptr_t virt __attribute__((unused))) {
    return &page_table_root;
}

void switch_to_runtime_page_tables()
{
    processor::write_cr3(page_table_root.next_pt_addr());
}

enum {
    page_fault_prot  = 1ul << 0,
    page_fault_write = 1ul << 1,
    page_fault_user  = 1ul << 2,
    page_fault_rsvd  = 1ul << 3,
    page_fault_insn  = 1ul << 4,
};

bool is_page_fault_insn(unsigned int error_code) {
    return error_code & page_fault_insn;
}

bool is_page_fault_write(unsigned int error_code) {
    return error_code & page_fault_write;
}

bool is_page_fault_rsvd(unsigned int error_code) {
    return error_code & page_fault_rsvd;
}

/* Glauber Costa: if page faults because we are trying to execute code here,
 * we shouldn't be closing the balloon. We should [...] despair.
 * So by checking only for == page_fault_write, we are guaranteed to close
 * the balloon in the next branch - which although still bizarre, at least
 * will give us tracing information that I can rely on for debugging that.
 * (the reason for that is that there are only fixups for memcpy, and memcpy
 * can only be used to read or write).
 * The other bits like present and user should not matter in this case.
 */
bool is_page_fault_write_exclusive(unsigned int error_code) {
    return error_code == page_fault_write;
}

bool is_page_fault_prot_write(unsigned int error_code) {
    return (error_code & (page_fault_write | page_fault_prot)) == (page_fault_write | page_fault_prot);
}

bool fast_sigsegv_check(uintptr_t addr, exception_frame* ef)
{
    if (is_page_fault_rsvd(ef->get_error())) {
        return true;
    }

    struct check_cow : public virt_pte_visitor {
        bool _result = false;
        void pte(pt_element<0> pte) override {
            _result = !pte_is_cow(pte) && !pte.writable();
        }
        void pte(pt_element<1> pte) override {
            // large ptes are never cow yet
        }
    } visitor;

    // if page is present, but write protected without cow bit set
    // it means that this address belong to PROT_READ vma, so no need
    // to search vma to verify permission
    if (is_page_fault_prot_write(ef->get_error())) {
        virt_visit_pte_rcu(addr, visitor);
        return visitor._result;
    }

    return false;
}
}
