/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef OSV_TICKETLOCK_H_
#define OSV_TICKETLOCK_H_

#include <sys/cdefs.h>
#include <cstddef>

__BEGIN_DECLS

// Spin lock. Use mutex instead, except where impossible:

typedef struct ticketlock {
    //unsigned int slock;
    size_t ticket;
    size_t cur;
#ifdef __cplusplus
    // additional convenience methods for C++
    inline constexpr ticketlock() : ticket(0), cur(0) { }
    inline bool trylock();
    inline void lock();
    inline void unlock();
#endif
} ticketlock_t;

static inline void ticketlock_init(ticketlock_t *tl)
{
    //tl->slock = 0;
    tl->ticket = 0;
    tl->cur = 0;
}
void ticket_lock(ticketlock_t *tl);
bool ticket_trylock(ticketlock_t *tl);
void ticket_unlock(ticketlock_t *tl);

__END_DECLS

#ifdef __cplusplus
void ticketlock::lock()
{
    ticket_lock(this);
}
void ticketlock::unlock()
{
    ticket_unlock(this);
}
#endif

#endif /* OSV_SPINLOCK_H_ */
