/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/ticketlock.h>
#include <osv/sched.hh>

void ticket_lock(ticketlock_t *tl)
{
    sched::preempt_disable();
    volatile size_t my_ticket = __sync_fetch_and_add((&tl->ticket), 1);
    while (my_ticket != tl->cur) {
        barrier();
    }
//     int inc = 0x00010000;
// 	int tmp;
// 
// 	asm volatile("xaddl %0, %1\n"
// 		     "movzwl %w0, %2\n\t"
// 		     "shrl $16, %0\n\t"
// 		     "1:\t"
// 		     "cmpl %0, %2\n\t"
// 		     "je 2f\n\t"
// 		     "rep ; nop\n\t"
// 		     "movzwl %1, %2\n\t"
// 		     /* don't need lfence here, because loads are in-order */
//              "lfence\n\t"
// 		     "jmp 1b\n"
// 		     "2:"
// 		     : "+r" (inc), "+m" (tl->slock), "=&r" (tmp)
// 		     :
// 		     : "memory", "cc");
}

// No trylock for ticket lock
bool ticket_trylock(ticketlock_t *tl)
{
    return false;
}

void ticket_unlock(ticketlock_t *tl)
{
    __sync_fetch_and_add((&tl->cur), 1);
    // asm volatile("incw %0"
		  //    : "+m" (tl->slock)
		  //    :
		  //    : "memory", "cc");
    sched::preempt_enable();
}
