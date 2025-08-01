/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "spinlock.h"
#include "helpers.hpp"

void spin_lock(spinlock_t *sl)
{
    while (__sync_lock_test_and_set(&sl->_lock, 1)) {
        while (sl->_lock) {
            barrier();
        }
    }
}

bool spin_trylock(spinlock_t *sl)
{
    if (__sync_lock_test_and_set(&sl->_lock, 1)) {
        return false;
    }
    return true;
}

void spin_unlock(spinlock_t *sl)
{
    __sync_lock_release(&sl->_lock, 0);
}
