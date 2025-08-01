#pragma once

#include <atomic>

#define MIND_MAX_THREAD 10
#define MIND_MAX_BLADE 8
#define MIND_NUM_MAX_THREAD (MIND_MAX_THREAD * MIND_MAX_BLADE)

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12
#endif
#ifndef CACHELINE
#define CACHELINE 64
#endif

enum{
    MIND_UNLOCKED = 0,
    MIND_LOCKED = 1
};

typedef struct alignas(CACHELINE)
{
    // std::atomic<uint64_t> lock[MIND_MAX_THREAD];
    uint64_t lock[MIND_MAX_THREAD];
}alignedLock;

typedef struct MindLock
{
    alignedLock blade_lock[MIND_MAX_BLADE];
    // std::atomic<uint64_t> lock[MIND_NUM_MAX_THREAD];
    volatile uint64_t dummy;
} mindlock_t;

void initMindLockSystem(void);
void initMindLock(struct MindLock *lock);
bool tryMindLock(struct MindLock *lock, int blade_id, int thread_id);
void tryMindUnlock(struct MindLock *lock, int blade_id, int thread_id);

struct alignas(PAGE_SIZE) margin_4kb_t
{
    uint64_t margin;
};

struct metadata_t
{
    unsigned int node_mask[MIND_MAX_BLADE];
    unsigned int fini_node_step[MIND_MAX_BLADE];
    struct margin_4kb_t __margin[4];    // 16 KB gap
    alignas(PAGE_SIZE) std::atomic<uint64_t> shared_lock;
};