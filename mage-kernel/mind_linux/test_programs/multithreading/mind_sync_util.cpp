#include "mind_sync_util.hpp"
#include <unistd.h>
#include <string>

#define VERBOSE

//static std::atomic<uint64_t> local_thread_lock[MIND_MAX_THREAD];
/*
void initMindLockSystem(void)
{
    for (int j = 0; j < MIND_MAX_THREAD; j++)
        local_thread_lock[j].store(0);
}
*/

// ==Lock==
void initMindLock(mindlock_t *lock)
{
    if (lock)
    {
        for (int i = 0; i < MIND_MAX_BLADE; i++)
            for (int j = 0; j < MIND_MAX_THREAD; j++)
                // lock->blade_lock[i].lock[j].store(MIND_UNLOCKED);
                lock->blade_lock[i].lock[j] = MIND_UNLOCKED;
    }
}

#if 0
static bool isNoOthers(mindlock_t *lock, int blade_id)
{
    for (int i=0; i < MIND_MAX_BLADE; i++)
    {
        if (i != blade_id && lock->blade_lock[i].lock.load() > 0)
        {
            volatile uint64_t debug = lock->blade_lock[i].lock.load();
            return false;
        }
    }
    // if (lock->thread_lock.load() > 0)
    // {
    //     return false;
    // }
    return true;
}
#endif

static bool isNoOthers(mindlock_t *lock, int blade_id, int thread_id)
{
    // local
    /*
    for (int j=0; j < MIND_MAX_THREAD; j++)
    {
        if ((j != thread_id) && local_thread_lock[j].load() == (uint64_t)lock)
        {
            return false;
        }
    }
    */

    // global
    for (int i=0; i < MIND_MAX_BLADE; i++)
    {
/*
#if 0
        for (int j=0; j < MIND_MAX_THREAD; j++)
        {
            // if ((i != blade_id || j != thread_id) && lock->blade_lock[i].lock[j].load() > 0)
            // {
            //     volatile uint64_t debug = lock->blade_lock[i].lock[j].load();
            //     return false;
            // }
            lock->dummy = 0;    // get it as writable page
            if ((i != blade_id || j != thread_id) && lock->blade_lock[i].lock[j] > 0)
            {
                volatile uint64_t debug = lock->blade_lock[i].lock[j];
                return false;
            }
        }
#endif
*/
        if ((i != blade_id) && lock->blade_lock[i].lock[0] == MIND_LOCKED)
        {
            volatile uint64_t debug = lock->blade_lock[i].lock[0];
            return false;
        }
    }
    return true;
}

bool tryMindLock(mindlock_t *lock, int blade_id, int thread_id)
{
    while(1)
    {
        uint64_t locked_val = (uint64_t)1 << thread_id, zero_val = 0;
        if (!isNoOthers(lock, blade_id, thread_id))
        {
            // already locked
            // tryMindUnlock(lock, blade_id, thread_id);
            return false;
        }

        asm volatile("mfence" : : : "memory");

        lock->blade_lock[blade_id].lock[0] = MIND_LOCKED;
        
        asm volatile("mfence" : : : "memory");

        if (!isNoOthers(lock, blade_id, thread_id))
        {
            // already locked
            // tryMindUnlock(lock, blade_id, thread_id);
            lock->blade_lock[blade_id].lock[0] = MIND_UNLOCKED;
            return false;
        }

        return true;

        // Lock in this machine
        // if (lock->blade_lock[blade_id].lock.compare_exchange_strong(zero_val, locked_val))
        // asm volatile("mfence" : : : "memory");
        //std::atomic_thread_fence(std::memory_order_seq_cst);
        // lock->blade_lock[blade_id].lock[thread_id].store(MIND_LOCKED);
        //local_thread_lock[thread_id].store((uint64_t)lock);
        // asm volatile("mfence" : : : "memory");
        //std::atomic_thread_fence(std::memory_order_seq_cst);
        /*
        {
            // Lock for other machines
            if (!isNoOthers(lock, blade_id, thread_id))
            {
                // tryMindUnlock(lock, blade_id, thread_id);
                local_thread_lock[thread_id].store(0);
                continue;
            }else{
                lock->blade_lock[blade_id].lock[0] = MIND_LOCKED;
                asm volatile("mfence" : : : "memory");
                return true;
            }
        }
        */
        // else
        // {
        //     // must not be itself
        //     locked_val = lock->blade_lock[blade_id].lock.load() & (~((uint64_t)1 << thread_id));
        //     while (!lock->blade_lock[blade_id].lock.compare_exchange_strong(zero_val, locked_val))
        //     {
        //         locked_val = zero_val & (~((uint64_t)1 << thread_id));
        //     }
        //     return false;
        // }
    }
    return false;   // failed
}

void tryMindUnlock(struct MindLock *lock, int blade_id, int thread_id)
{
    asm volatile("mfence" : : : "memory");
    // lock->blade_lock[blade_id].lock[thread_id].store(MIND_UNLOCKED);
    lock->blade_lock[blade_id].lock[0] = 0;
    //local_thread_lock[thread_id].store(0);
    //
    asm volatile("mfence" : : : "memory");
}