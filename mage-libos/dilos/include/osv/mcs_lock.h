/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef OSV_MCSLOCK_H_
#define OSV_MCSLOCK_H_

#include <sys/cdefs.h>
#include <osv/debug.hh>
#include <cstddef>
__BEGIN_DECLS

// Should be 16 bytes
struct mcs_spinlock{
    struct mcs_spinlock* next;
    int locked;
    int count;
};
#define MAX_NODES 4
#define MAX_CPUS 56

// Spin lock. Use mutex instead, except where impossible:

typedef struct mcslock {
    // Avoid false sharing
    struct mcs_spinlock lock_node[MAX_CPUS * MAX_NODES] __attribute__((aligned(64)));
    struct mcs_spinlock *lock_word;
#ifdef __cplusplus
    // additional convenience methods for C++
    inline mcslock() : lock_word(NULL) {
        for(int i = 0; i < MAX_CPUS * MAX_NODES; i++){
            lock_node[i].next = 0;
            lock_node[i].locked = 0;
            lock_node[i].count = 0;
        }
    }
    inline bool trylock();
    inline void lock();
    inline void unlock();
#endif
} mcslock_t;

static inline void mcslock_init(mcslock_t *ml)
{
    ml->lock_word = NULL;
}
void mcs_lock(mcslock_t *ml);
bool mcs_trylock(mcslock_t *ml);
void mcs_unlock(mcslock_t *ml);

__END_DECLS

#ifdef __cplusplus
void mcslock::lock()
{
    mcs_lock(this);
}
void mcslock::unlock()
{
    mcs_unlock(this);
}
#endif

#endif /* OSV_SPINLOCK_H_ */
