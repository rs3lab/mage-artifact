/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/mcs_lock.h>
#include <osv/sched.hh>

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

#define READ_ONCE(x) \
({ typeof(x) ___x = ACCESS_ONCE(x); ___x; })

#define WRITE_ONCE(x, val) \
do { ACCESS_ONCE(x) = (val); } while (0)

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

void mcs_lock(mcslock_t *ml)
{
    sched::preempt_disable();
    auto cpu_id = sched::cpu::current() == nullptr ? 0 : sched::cpu::current()->id;
    struct mcs_spinlock *node = &ml->lock_node[cpu_id * MAX_NODES];
    struct mcs_spinlock *prev;
    /* Init node */
    /* Should not have been inited */
    node->locked = 0;
    node->next = NULL;

    prev = __sync_lock_test_and_set(&ml->lock_word, node);
    if (likely(prev == NULL)){
        return;
    }
    WRITE_ONCE(prev->next, node);

    //smp_cond_load_acquire
    int val;
    for(;;){
        val = READ_ONCE(node->locked);
        if (val) break;
        // CPU relax
        asm volatile("rep; nop");
    }
    barrier();
}

// No trylock for mcs lock
bool mcs_trylock(mcslock_t *ml)
{
    return false;
}

void mcs_unlock(mcslock_t *ml)
{
    auto cpu_id = sched::cpu::current() == nullptr ? 0 : sched::cpu::current()->id;
    struct mcs_spinlock *node = &ml->lock_node[cpu_id * MAX_NODES];
    struct mcs_spinlock *next = READ_ONCE(node->next);

    if(likely(!next)){
        if(likely(__sync_val_compare_and_swap(&ml->lock_word, node, NULL) == node)){
            sched::preempt_enable();
            return;
        }
        while(!(next = READ_ONCE(node->next)))
            // CPU relax
            asm volatile("rep; nop");
    }
    barrier();
    // Pass the lock
    WRITE_ONCE(next->locked, 1);
    sched::preempt_enable();
}
