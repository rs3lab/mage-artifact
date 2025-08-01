#ifndef __RANGE_LOCK_DISAGGREGATION_H__
#define __RANGE_LOCK_DISAGGREGATION_H__

#include <linux/slab.h>
#include <linux/hash.h>
#include <disagg/print_disagg.h>
#include <disagg/config.h>
#include <disagg/profile_points_disagg.h>
#include <linux/sched.h>
#include <linux/interval_tree.h>
#include <linux/rbtree_augmented.h>
#include <linux/delay.h>
#include <asm/tlb.h>

// ----------------------------------------------------------------------------
//   API and Struct Definitions
// ----------------------------------------------------------------------------

// DECLARE_PP(RLT_total);
// DECLARE_PP(RLT_lock_tree);

struct cnpage_lock {
    struct interval_tree_node node;
    atomic_t *holds_range_lock;
};

struct cnpage_lock_tree {
    struct rb_root_cached root;
    spinlock_t lock;
};

struct cnpage_lock_bucket {
    // y: Later, I could replace this with my own lighter weight implementation
    //    as required. The key point is: each bucket should have its own lock.
    struct cnpage_lock_tree range_lock_root;
};


extern struct cnpage_lock_bucket cnpage_lock_table[CNPAGE_LOCK_TABLE_SIZE];

void init_cnpage_range_locks(void);
static int cnpage_try_lock_range(u16 tgid, u64 start, u64 end, struct cnpage_lock *lock);
static void cnpage_lock_range(u16 tgid, u64 start, u64 end, struct cnpage_lock *lock);
static void cnpage_unlock_range(u16 tgid, struct cnpage_lock *lock);

// ----------------------------------------------------------------------------
//   Inline Implementation
// ----------------------------------------------------------------------------

#define cnpage_for_each_overlapping_lock(cur, root, start, last)	\
        for (cur = interval_tree_iter_first(root, start, last);         \
             cur;                                                       \
             cur = interval_tree_iter_next(cur, start, last))

static void __maybe_unused cnpage_lock_tree_init(struct cnpage_lock_tree *tree)
{
    tree->root = RB_ROOT_CACHED;
    spin_lock_init(&tree->lock);
}

static void __maybe_unused __cnpage_lock_init(struct cnpage_lock *lock, u64 start, u64 end)
{
    BUG_ON(end <= start);
    RB_CLEAR_NODE(&lock->node.rb);
    lock->node.start = start;
    lock->node.last = end - 1;
    lock->holds_range_lock = &current->holds_range_lock;
    atomic_set(lock->holds_range_lock, 0);
}

// Delay until another thread gives the lock to us.
static void __maybe_unused __cnpage_wait_for_lock(struct cnpage_lock *lock)
{
    while (atomic_read(lock->holds_range_lock) == 0)
        ndelay(50);
}

extern void patr_poll_fh_queue(void);

// Delay until another thread gives the lock to us.
// Spend the delay time handling PATR invalidations.
static void __maybe_unused __cnpage_patr_wait_for_lock(struct cnpage_lock *lock)
{
    while (atomic_read(lock->holds_range_lock) == 0) {
        ndelay(25);
        patr_poll_fh_queue();
    }
}

// Adds our lock to the interval tree. If the lock was successfully acquired, returns `true`.
// Otherwise, the caller should wait until `lock->holds_range_lock` is set to true
// (=> someone else has given the caller the lock).
static bool __maybe_unused __cnpage_enqueue_lock(struct cnpage_lock_tree *tree, struct cnpage_lock *lock)
{
    struct interval_tree_node *cur = NULL;
    bool already_locked = false;

    spin_lock(&tree->lock);
    cnpage_for_each_overlapping_lock(cur, &tree->root, lock->node.start, lock->node.last) {
        already_locked = true;
        break;
    }
    interval_tree_insert(&lock->node, &tree->root);

    if (!already_locked) {
        atomic_set(lock->holds_range_lock, 1);
        spin_unlock(&tree->lock);
        return true;
    }
    atomic_set(lock->holds_range_lock, 0);
    spin_unlock(&tree->lock);
    return false;
}

// Returns 1 if the lock was acquired successfully, and 0 on contention.
static __maybe_unused int __cnpage_trylock(struct cnpage_lock_tree *tree, struct cnpage_lock *lock)
{
    struct interval_tree_node *cur;
    // PP_STATE(RLT_lock_tree);

    // PP_ENTER(RLT_lock_tree);
    spin_lock(&tree->lock);
    // PP_EXIT(RLT_lock_tree);

    cnpage_for_each_overlapping_lock(cur, &tree->root, lock->node.start, lock->node.last) {
        spin_unlock(&tree->lock);
        return 0; // contention!
    }

    interval_tree_insert(&lock->node, &tree->root);
    atomic_set(&current->holds_range_lock, 1);
    spin_unlock(&tree->lock);
    return 1; // success!
}

static __maybe_unused void __cnpage_unlock(struct cnpage_lock_tree *tree, struct cnpage_lock *lock)
{
    struct interval_tree_node *cur;

    spin_lock(&tree->lock);
    interval_tree_remove(&lock->node, &tree->root);

    // Free one other lock blocking on this range.
    cnpage_for_each_overlapping_lock(cur, &tree->root, lock->node.start, lock->node.last)
    {
        struct cnpage_lock *cur_lock = container_of(cur, struct cnpage_lock, node);
        atomic_set(cur_lock->holds_range_lock, 1);
        break;
    }
    spin_unlock(&tree->lock);
    return;
}


// XXX(yash): This hash scheme ignores TGIDs; so one address space can lock the
//            other. This could cause deadlocks; fix it as needed.
//
// Remember that the old tgid hash function was masked:
// `static u16 hash_ftn(u16 tgid) { return (tgid & MAX_PROCESS_BUCKET_MASK); }`
static __maybe_unused struct cnpage_lock_bucket *get_bucket(u16 tgid, unsigned long addr)
{
    return &cnpage_lock_table[hash_64(addr, CNPAGE_LOCK_TABLE_BITS)];
}

// For now: assumes all locks are page size.
static inline __maybe_unused void cnpage_lock_range(u16 tgid, u64 start, u64 end, struct cnpage_lock *lock) {
    struct cnpage_lock_bucket *bucket = get_bucket(tgid, start);
    bool lock_acquired;
    __cnpage_lock_init(lock, start, end);

    lock_acquired = __cnpage_enqueue_lock(&bucket->range_lock_root, lock);
    if (!lock_acquired)
         __cnpage_wait_for_lock(lock);
}

// Locks a section of the address space. Polls the PATR queue while spinning on lock acquisition.
// For now: assumes all locks are page size.
static inline __maybe_unused void cnpage_lock_range__patr_delay(u16 tgid,
        u64 start, u64 end, struct cnpage_lock *lock)
{
    struct cnpage_lock_bucket *bucket = get_bucket(tgid, start);
    bool lock_acquired;
    __cnpage_lock_init(lock, start, end);

    lock_acquired = __cnpage_enqueue_lock(&bucket->range_lock_root, lock);
    if (!lock_acquired)
         __cnpage_patr_wait_for_lock(lock);
}

static inline __maybe_unused void cnpage_unlock_range(u16 tgid, struct cnpage_lock *lock) {
    struct cnpage_lock_bucket *bucket;
    // PP_STATE(RLU_get_bucket);

    // PP_ENTER(RLU_get_bucket);
    bucket = get_bucket(tgid, lock->node.start);
    // PP_EXIT(RLU_get_bucket);
    __cnpage_unlock(&bucket->range_lock_root, lock);
}

/* returns 1 if successful */
static inline __maybe_unused int cnpage_try_lock_range(u16 tgid, u64 start, u64 end, struct cnpage_lock *lock)
{
    struct cnpage_lock_bucket *bucket;
    int ret;
    // PP_STATE(RLT_total);

    // PP_ENTER(RLT_total);
    bucket = get_bucket(tgid, start);
    __cnpage_lock_init(lock, start, end);
    ret = __cnpage_trylock(&bucket->range_lock_root, lock);
    // PP_EXIT(RLT_total);

    return ret;
}


#endif  /* __RANGE_LOCK_DISAGGREGATION_H__ */
