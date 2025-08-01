/*
 * Copyright (C) 2017 Jan Kara, Davidlohr Bueso.
 */

#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/range_lock.h>
#include <linux/lockdep.h>
#include <linux/sched/signal.h>
#include <linux/sched/debug.h>
#include <linux/sched/wake_q.h>
#include <linux/sched.h>
#include <linux/export.h>
#include <disagg/profile_points_disagg.h>

// DEFINE_PP(RLL_wait);
// DEFINE_PP(RLU_walk_tree);
// DEFINE_PP(RLU_lock_tree);
// DEFINE_PP(RLU_wake_up_waiters);

/* Given an interval, iterate over all nodes that might intersect with it. */
#define range_interval_tree_foreach(node, root, start, last)	\
	for (node = interval_tree_iter_first(root, start, last); \
	     node; node = interval_tree_iter_next(node, start, last))

#define to_range_lock(ptr) container_of(ptr, struct range_lock, node)
#define to_interval_tree_node(ptr) \
	container_of(ptr, struct interval_tree_node, rb)

static inline void
__range_tree_insert(struct range_lock_tree *tree, struct range_lock *lock)
{
	lock->seqnum = tree->seqnum++;
	interval_tree_insert(&lock->node, &tree->root);
}

static inline void
__range_tree_remove(struct range_lock_tree *tree, struct range_lock *lock)
{
	interval_tree_remove(&lock->node, &tree->root);
}

/*
 * lock->tsk reader tracking.
 */
#define RANGE_FLAG_READER	1UL

/* y: Returns the task that currently holds the range_lock. */
static inline struct task_struct *range_lock_waiter(struct range_lock *lock)
{
	return (struct task_struct *)
		((unsigned long) lock->tsk & ~RANGE_FLAG_READER);
}

static inline void range_lock_set_reader(struct range_lock *lock)
{
	lock->tsk = (struct task_struct *)
		((unsigned long)lock->tsk | RANGE_FLAG_READER);
}

static inline void range_lock_clear_reader(struct range_lock *lock)
{
	lock->tsk = (struct task_struct *)
		((unsigned long)lock->tsk & ~RANGE_FLAG_READER);
}

static inline bool range_lock_is_reader(struct range_lock *lock)
{
	return (unsigned long) lock->tsk & RANGE_FLAG_READER;
}

static inline void
__range_lock_init(struct range_lock *lock,
		  unsigned long start, unsigned long last)
{
	WARN_ON(start > last);

	lock->node.start = start;
	lock->node.last = last;
	RB_CLEAR_NODE(&lock->node.rb);
	lock->blocking_ranges = 0;
	lock->tsk = NULL;
	lock->seqnum = 0;
}

/**
 * range_lock_init - Initialize a range lock
 * @lock: the range lock to be initialized
 * @start: start of the interval (inclusive)
 * @last: last location in the interval (inclusive)
 *
 * Initialize the range's [start, last] such that it can
 * later be locked. User is expected to enter a sorted
 * range, such that @start <= @last.
 *
 * It is not allowed to initialize an already locked range.
 */
void range_lock_init(struct range_lock *lock,
		     unsigned long start, unsigned long last)
{
	__range_lock_init(lock, start, last);
}
EXPORT_SYMBOL_GPL(range_lock_init);

/**
 * range_lock_init_full - Initialize a full range lock
 * @lock: the range lock to be initialized
 *
 * Initialize the full range.
 *
 * It is not allowed to initialize an already locked range.
 */
void range_lock_init_full(struct range_lock *lock)
{
	__range_lock_init(lock, 0, RANGE_LOCK_FULL);
}
EXPORT_SYMBOL_GPL(range_lock_init_full);

static inline void
range_lock_put(struct range_lock *lock, struct wake_q_head *wake_q)
{
	if (!--lock->blocking_ranges)
		wake_q_add(wake_q, range_lock_waiter(lock));
}

static inline int wait_for_ranges(struct range_lock_tree *tree,
				  struct range_lock *lock, long state)
{
	int ret = 0;

	while (true) {
		set_current_state(state);

		/* do we need to go to sleep? */
		if (!lock->blocking_ranges)
			break;

		if (unlikely(signal_pending_state(state, current))) {
			struct interval_tree_node *node;
			DEFINE_WAKE_Q(wake_q);

			ret = -EINTR;
			/*
			 * We're not taking the lock after all, cleanup
			 * after ourselves.
			 */
			spin_lock(&tree->lock);

			range_lock_clear_reader(lock);
			__range_tree_remove(tree, lock);

			range_interval_tree_foreach(node, &tree->root,
						    lock->node.start,
						    lock->node.last) {
				struct range_lock *blked;
				blked = to_range_lock(node);

				if (range_lock_is_reader(lock) &&
				    range_lock_is_reader(blked))
					continue;

				/* unaccount for threads _we_ are blocking */
				if (lock->seqnum < blked->seqnum)
					range_lock_put(blked, &wake_q);
			}

			spin_unlock(&tree->lock);
			wake_up_q(&wake_q);
			break;
		}

		schedule();
	}

	__set_current_state(TASK_RUNNING);
	return ret;
}

/**
 * range_read_trylock - Trylock for reading
 * @tree: interval tree
 * @lock: the range lock to be trylocked
 *
 * The trylock is against the range itself, not the @tree->lock.
 *
 * Returns 1 if successful, 0 if contention (must block to acquire).
 */
static inline int __range_read_trylock(struct range_lock_tree *tree,
				       struct range_lock *lock)
{
	int ret = true;
	struct interval_tree_node *node;

	spin_lock(&tree->lock);

	range_interval_tree_foreach(node, &tree->root,
				    lock->node.start, lock->node.last) {
		struct range_lock *blocked_lock;
		blocked_lock = to_range_lock(node);

		if (!range_lock_is_reader(blocked_lock)) {
			ret = false;
			goto unlock;
		}
	}

	range_lock_set_reader(lock);
	__range_tree_insert(tree, lock);
unlock:
	spin_unlock(&tree->lock);

	return ret;
}

int range_read_trylock(struct range_lock_tree *tree, struct range_lock *lock)
{
	int ret = __range_read_trylock(tree, lock);

	if (ret)
		range_lock_acquire_read(&tree->dep_map, 0, 1, _RET_IP_);

	return ret;
}

EXPORT_SYMBOL_GPL(range_read_trylock);

static __always_inline int __sched
__range_read_lock_common(struct range_lock_tree *tree,
			 struct range_lock *lock, long state)
{
	struct interval_tree_node *node;

	spin_lock(&tree->lock);

	range_interval_tree_foreach(node, &tree->root,
				    lock->node.start, lock->node.last) {
		struct range_lock *blocked_lock;
		blocked_lock = to_range_lock(node);

		if (!range_lock_is_reader(blocked_lock))
			lock->blocking_ranges++;
	}

	__range_tree_insert(tree, lock);

	lock->tsk = current;
	range_lock_set_reader(lock);
	spin_unlock(&tree->lock);

	return wait_for_ranges(tree, lock, state);
}

static __always_inline int
__range_read_lock(struct range_lock_tree *tree, struct range_lock *lock)
{
	return __range_read_lock_common(tree, lock, TASK_UNINTERRUPTIBLE);
}

/**
 * range_read_lock - Lock for reading
 * @tree: interval tree
 * @lock: the range lock to be locked
 *
 * Returns when the lock has been acquired or sleep until
 * until there are no overlapping ranges.
 */
void range_read_lock(struct range_lock_tree *tree, struct range_lock *lock)
{
	might_sleep();
	range_lock_acquire_read(&tree->dep_map, 0, 0, _RET_IP_);

	RANGE_LOCK_CONTENDED(tree, lock,
			     __range_read_trylock, __range_read_lock);
}
EXPORT_SYMBOL_GPL(range_read_lock);

/**
 * range_read_lock_interruptible - Lock for reading (interruptible)
 * @tree: interval tree
 * @lock: the range lock to be locked
 *
 * Lock the range like range_read_lock(), and return 0 if the
 * lock has been acquired or sleep until until there are no
 * overlapping ranges. If a signal arrives while waiting for the
 * lock then this function returns -EINTR.
 */
int range_read_lock_interruptible(struct range_lock_tree *tree,
				  struct range_lock *lock)
{
	might_sleep();
	return __range_read_lock_common(tree, lock, TASK_INTERRUPTIBLE);
}
EXPORT_SYMBOL_GPL(range_read_lock_interruptible);

/**
 * range_read_lock_killable - Lock for reading (killable)
 * @tree: interval tree
 * @lock: the range lock to be locked
 *
 * Lock the range like range_read_lock(), and return 0 if the
 * lock has been acquired or sleep until until there are no
 * overlapping ranges. If a signal arrives while waiting for the
 * lock then this function returns -EINTR.
 */
static __always_inline int
__range_read_lock_killable(struct range_lock_tree *tree,
			   struct range_lock *lock)
{
	return __range_read_lock_common(tree, lock, TASK_KILLABLE);
}

int range_read_lock_killable(struct range_lock_tree *tree,
			     struct range_lock *lock)
{
	might_sleep();
	range_lock_acquire_read(&tree->dep_map, 0, 0, _RET_IP_);

	if (RANGE_LOCK_CONTENDED_RETURN(tree, lock, __range_read_trylock,
					__range_read_lock_killable)) {
		range_lock_release(&tree->dep_map, 1, _RET_IP_);
		return -EINTR;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(range_read_lock_killable);

/**
 * range_read_unlock - Unlock for reading
 * @tree: interval tree
 * @lock: the range lock to be unlocked
 *
 * Wakes any blocked readers, when @lock is the only conflicting range.
 *
 * It is not allowed to unlock an unacquired read lock.
 */
void range_read_unlock(struct range_lock_tree *tree, struct range_lock *lock)
{
	struct interval_tree_node *node;
	DEFINE_WAKE_Q(wake_q);

	spin_lock(&tree->lock);

	range_lock_clear_reader(lock);
	__range_tree_remove(tree, lock);

	range_lock_release(&tree->dep_map, 1, _RET_IP_);

	range_interval_tree_foreach(node, &tree->root,
				    lock->node.start, lock->node.last) {
		struct range_lock *blocked_lock;
		blocked_lock = to_range_lock(node);

		if (!range_lock_is_reader(blocked_lock))
			range_lock_put(blocked_lock, &wake_q);
	}

	spin_unlock(&tree->lock);
	wake_up_q(&wake_q);
}
EXPORT_SYMBOL_GPL(range_read_unlock);

/*
 * Check for overlaps for fast write_trylock(), which is the same
 * optimization that interval_tree_iter_first() does.
 */
static inline bool __range_overlaps_intree(struct range_lock_tree *tree,
					   struct range_lock *lock)
{
	struct interval_tree_node *root;
	struct range_lock *left;

	if (unlikely(RB_EMPTY_ROOT(&tree->root.rb_root)))
		return false;

	root = to_interval_tree_node(tree->root.rb_root.rb_node);
	left = to_range_lock(to_interval_tree_node(rb_first_cached(&tree->root)));

	return lock->node.start <= root->__subtree_last &&
		left->node.start <= lock->node.last;
}

/**
 * range_write_trylock - Trylock for writing
 * @tree: interval tree
 * @lock: the range lock to be trylocked
 *
 * The trylock is against the range itself, not the @tree->lock.
 *
 * Returns 1 if successful, 0 if contention (must block to acquire).
 */
int range_write_trylock(struct range_lock_tree *tree,
					struct range_lock *lock)
{
	bool lock_is_contended = false;
	struct interval_tree_node *node;

	spin_lock(&tree->lock);

	range_interval_tree_foreach(node, &tree->root,
			lock->node.start, lock->node.last) {
		struct range_lock *blocked_lock;
		blocked_lock = to_range_lock(node);

		// If any node overlaps, set ret to 0 and break
		if (lock->node.start <= blocked_lock->node.last &&
				lock->node.last >= blocked_lock->node.start) {
			lock_is_contended = 1;
			break;
		}
	}

	if (!lock_is_contended) {
		range_lock_clear_reader(lock);
		__range_tree_insert(tree, lock);
		range_lock_acquire(&tree->dep_map, 0, 1, _RET_IP_);
	}

	spin_unlock(&tree->lock);

	return !lock_is_contended;
}
EXPORT_SYMBOL_GPL(range_write_trylock);

static __always_inline int __sched
__range_write_lock_common(struct range_lock_tree *tree,
			  struct range_lock *lock, long state)
{
	// PP_STATE(RLL_walk_tree);
	// PP_STATE(RLL_lock_tree);
	// PP_STATE(RLL_wait);

	struct interval_tree_node *node;
	int ret;

	// PP_ENTER(RLL_walk_tree);
	// PP_ENTER(RLL_lock_tree);
	spin_lock(&tree->lock);
	// PP_EXIT(RLL_lock_tree);

	range_interval_tree_foreach(node, &tree->root,
				    lock->node.start, lock->node.last) {
		/*
		 * As a writer, we always consider an existing node. We
		 * need to wait; either the intersecting node is another
		 * writer or we have a reader that needs to finish.
		 */
		lock->blocking_ranges++;
	}

	__range_tree_insert(tree, lock);

	lock->tsk = current;
	spin_unlock(&tree->lock);
	// PP_EXIT(RLL_walk_tree);

	// PP_ENTER(RLL_wait);
	ret = wait_for_ranges(tree, lock, state);
	// PP_EXIT(RLL_wait);

	return ret;
}

static __always_inline int
__range_write_lock(struct range_lock_tree *tree, struct range_lock *lock)
{
	return __range_write_lock_common(tree, lock, TASK_UNINTERRUPTIBLE);
}

/**
 * range_write_lock - Lock for writing
 * @tree: interval tree
 * @lock: the range lock to be locked
 *
 * Returns when the lock has been acquired or sleep until
 * until there are no overlapping ranges.
 */
void range_write_lock(struct range_lock_tree *tree, struct range_lock *lock)
{
	pr_rlock("RLOCK: spin locking 0x%lx-0x%lx\n", lock->node.start, lock->node.last);
	might_sleep();
	// lockdep
	range_lock_acquire(&tree->dep_map, 0, 0, _RET_IP_);

        __range_write_lock(tree, lock);
        pr_rlock("RLOCK: done spin locking 0x%lx-0x%ld\n", lock->node.start, lock->node.last);
}
EXPORT_SYMBOL_GPL(range_write_lock);

/**
 * range_write_lock_interruptible - Lock for writing (interruptible)
 * @tree: interval tree
 * @lock: the range lock to be locked
 *
 * Lock the range like range_write_lock(), and return 0 if the
 * lock has been acquired or sleep until until there are no
 * overlapping ranges. If a signal arrives while waiting for the
 * lock then this function returns -EINTR.
 */
int range_write_lock_interruptible(struct range_lock_tree *tree,
				   struct range_lock *lock)
{
	might_sleep();
	return __range_write_lock_common(tree, lock, TASK_INTERRUPTIBLE);
}
EXPORT_SYMBOL_GPL(range_write_lock_interruptible);

/**
 * range_write_lock_killable - Lock for writing (killable)
 * @tree: interval tree
 * @lock: the range lock to be locked
 *
 * Lock the range like range_write_lock(), and return 0 if the
 * lock has been acquired or sleep until until there are no
 * overlapping ranges. If a signal arrives while waiting for the
 * lock then this function returns -EINTR.
 */
static __always_inline int
__range_write_lock_killable(struct range_lock_tree *tree,
			   struct range_lock *lock)
{
	return __range_write_lock_common(tree, lock, TASK_KILLABLE);
}

int range_write_lock_killable(struct range_lock_tree *tree,
			      struct range_lock *lock)
{
	might_sleep();
	range_lock_acquire(&tree->dep_map, 0, 0, _RET_IP_);

	if (RANGE_LOCK_CONTENDED_RETURN(tree, lock, __range_write_trylock,
					__range_write_lock_killable)) {
		range_lock_release(&tree->dep_map, 1, _RET_IP_);
		return -EINTR;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(range_write_lock_killable);

/**
 * range_write_unlock - Unlock for writing
 * @tree: interval tree
 * @lock: the range lock to be unlocked
 *
 * Wakes any blocked readers, when @lock is the only conflicting range.
 *
 * It is not allowed to unlock an unacquired write lock.
 */
void range_write_unlock(struct range_lock_tree *tree, struct range_lock *lock)
{
	struct interval_tree_node *node;
	// PP_STATE(RLU_walk_tree);
	// PP_STATE(RLU_lock_tree);
	// PP_STATE(RLU_wake_up_waiters);

	DEFINE_WAKE_Q(wake_q);

	pr_rlock("RLOCK: unlock 0x%lx-0x%lx\n", lock->node.start, lock->node.last);

	// PP_ENTER(RLU_walk_tree);
	// PP_ENTER(RLU_lock_tree);
	spin_lock(&tree->lock);
	// PP_EXIT(RLU_lock_tree);

	range_lock_clear_reader(lock);
	__range_tree_remove(tree, lock);

	range_lock_release(&tree->dep_map, 1, _RET_IP_);

	range_interval_tree_foreach(node, &tree->root,
				    lock->node.start, lock->node.last) {
		struct range_lock *blocked_lock;
		blocked_lock = to_range_lock(node);

		range_lock_put(blocked_lock, &wake_q);
	}

	spin_unlock(&tree->lock);
	// PP_EXIT(RLU_walk_tree);

	// PP_ENTER(RLU_wake_up_waiters);
	wake_up_q(&wake_q);
	// PP_EXIT(RLU_wake_up_waiters);
}
EXPORT_SYMBOL_GPL(range_write_unlock);

/**
 * range_downgrade_write - Downgrade write range lock to read lock
 * @tree: interval tree
 * @lock: the range lock to be downgraded
 *
 * Wakes any blocked readers, when @lock is the only conflicting range.
 *
 * It is not allowed to downgrade an unacquired write lock.
 */
void range_downgrade_write(struct range_lock_tree *tree,
			   struct range_lock *lock)
{
	struct interval_tree_node *node;
	DEFINE_WAKE_Q(wake_q);

	lock_downgrade(&tree->dep_map, _RET_IP_);

	spin_lock(&tree->lock);

	WARN_ON(range_lock_is_reader(lock));

	range_interval_tree_foreach(node, &tree->root,
				    lock->node.start, lock->node.last) {
		struct range_lock *blocked_lock;
		blocked_lock = to_range_lock(node);

		/*
		 * Unaccount for any blocked reader lock. Wakeup if possible.
		 */
		if (range_lock_is_reader(blocked_lock))
			range_lock_put(blocked_lock, &wake_q);
	}

	range_lock_set_reader(lock);
	spin_unlock(&tree->lock);
	wake_up_q(&wake_q);
}
EXPORT_SYMBOL_GPL(range_downgrade_write);

/**
 * range_is_locked - Returns 1 if the given range is already either reader or
 *                   writer owned. Otherwise 0.
 * @tree: interval tree
 * @lock: the range lock to be checked
 *
 * Similar to trylocks, this is against the range itself, not the @tree->lock.
 */
int range_is_locked(struct range_lock_tree *tree, struct range_lock *lock)
{
	int overlaps;

	spin_lock(&tree->lock);
	overlaps = __range_overlaps_intree(tree, lock);
	spin_unlock(&tree->lock);

	return overlaps;
}
EXPORT_SYMBOL_GPL(range_is_locked);

#ifdef CONFIG_DEBUG_LOCK_ALLOC

void range_read_lock_nested(struct range_lock_tree *tree,
			    struct range_lock *lock, int subclass)
{
	might_sleep();
	range_lock_acquire_read(&tree->dep_map, subclass, 0, _RET_IP_);

	RANGE_LOCK_CONTENDED(tree, lock, __range_read_trylock, __range_read_lock);
}
EXPORT_SYMBOL_GPL(range_read_lock_nested);

void _range_write_lock_nest_lock(struct range_lock_tree *tree,
				struct range_lock *lock,
				struct lockdep_map *nest)
{
	might_sleep();
	range_lock_acquire_nest(&tree->dep_map, 0, 0, nest, _RET_IP_);

	RANGE_LOCK_CONTENDED(tree, lock,
			     __range_write_trylock, __range_write_lock);
}
EXPORT_SYMBOL_GPL(_range_write_lock_nest_lock);

void range_write_lock_nested(struct range_lock_tree *tree,
			    struct range_lock *lock, int subclass)
{
	might_sleep();
	range_lock_acquire(&tree->dep_map, subclass, 0, _RET_IP_);

	RANGE_LOCK_CONTENDED(tree, lock,
			     __range_write_trylock, __range_write_lock);
}
EXPORT_SYMBOL_GPL(range_write_lock_nested);


int range_write_lock_killable_nested(struct range_lock_tree *tree,
				     struct range_lock *lock, int subclass)
{
	might_sleep();
	range_lock_acquire(&tree->dep_map, subclass, 0, _RET_IP_);

	if (RANGE_LOCK_CONTENDED_RETURN(tree, lock, __range_write_trylock,
					__range_write_lock_killable)) {
		range_lock_release(&tree->dep_map, 1, _RET_IP_);
		return -EINTR;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(range_write_lock_killable_nested);
#endif
