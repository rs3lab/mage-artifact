#include "controller.h"
#include "cacheline_manager.h"
#include "memory_management.h"
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>	//close
#include <fcntl.h>  //socket configuration
#include <arpa/inet.h>
#include <sys/time.h>
#include "cacheline_def.h"
#include "cache_manager_thread.h"
#include "cacheline_manager.h"

static pthread_spinlock_t cacheman_usedlist_lock, cacheman_lock, cacheman_recovery_lock;
static int unlock_requested = 0;

void cache_man_init(void)
{
    pthread_spin_init(&cacheman_usedlist_lock, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&cacheman_lock, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&cacheman_recovery_lock, PTHREAD_PROCESS_PRIVATE);
}

void cacheman_run_lock()
{
    pthread_spin_lock(&cacheman_lock);
}

void cacheman_run_unlock()
{
    pthread_spin_unlock(&cacheman_lock);
}

void cacheman_request_unlock()
{
    unlock_requested = 1;
}

void cacheman_recover_lock()
{
    pthread_spin_lock(&cacheman_recovery_lock);
}

void cacheman_recover_unlock()
{
    pthread_spin_unlock(&cacheman_recovery_lock);
}


static void clear_cache_dir_no_lock(u64 fva)
{
    hlist *hash_node;
    hash_node = cacheline_get_node(fva);

    if (hash_node)
    {
        struct cacheline_dir *dir_ptr = (struct cacheline_dir *)hash_node->value;
        u16 shift = dir_ptr->dir_size + REGION_SIZE_BASE;
        u64 aligned_fva = get_aligned_fva(fva, shift);
        int c_idx = dir_ptr->idx;
        // Delete tcam and register
        pr_cache("Cacheline found - PID+VA: 0x%lx => Idx: %d\n", aligned_fva, c_idx);
        bfrt_del_cacheline(aligned_fva, 64 - shift);
        // Delete from hash list
        delete_from_hash_list(fva, hash_node);
        // Add back to free list
        if (dir_ptr->node_ptr)
        {
            delete_from_used_list(dir_ptr->node_ptr);
        }
        add_to_free_list(dir_ptr);
        check_and_print_cacheline(aligned_fva, c_idx);
    }else{
        // pr_cache("Cacheline not found - FVA: 0x%lx \n", fva);
    }
}

void try_clear_cache_dir(u64 fva)
{
    pthread_spin_lock(&cacheman_usedlist_lock);
    clear_cache_dir_no_lock(fva);
    pthread_spin_unlock(&cacheman_usedlist_lock);
}

static void clear_cache_dir_with_boundry_no_lock(u64 fva, u64 boundry)
{
    hlist *hash_node;
    hash_node = cacheline_get_node(fva);

    if (hash_node)
    {
        struct cacheline_dir *dir_ptr = (struct cacheline_dir *)hash_node->value;
        u16 shift = dir_ptr->dir_size + REGION_SIZE_BASE;
        u64 dir_size = (1 << shift);
        u64 aligned_fva = get_aligned_fva(fva, shift);
        int c_idx = dir_ptr->idx;
        // Delete tcam and register
        pr_cache("Cacheline found - PID+VA: 0x%lx => Idx: %d\n", aligned_fva, c_idx);

        if (aligned_fva + dir_size > boundry) {
            // split or skip?
            // skip for now.
            printf("skip cache entry free for fva: %lx, aligned fva :%lx, boundry: %lx\n", fva, aligned_fva, boundry);
        } else {
            bfrt_del_cacheline(aligned_fva, 64 - shift);
            // Delete from hash list
            delete_from_hash_list(fva, hash_node);
            // Add back to free list
            if (dir_ptr->node_ptr)
            {
                delete_from_used_list(dir_ptr->node_ptr);
            }
            add_to_free_list(dir_ptr);
            check_and_print_cacheline(aligned_fva, c_idx);
        }
    }else{
        // pr_cache("Cacheline not found - FVA: 0x%lx \n", fva);
    }
}

void try_clear_cache_dir_with_boundry(u64 fva, u64 boundry)
{
    pthread_spin_lock(&cacheman_usedlist_lock);
    clear_cache_dir_with_boundry_no_lock(fva, boundry);
    pthread_spin_unlock(&cacheman_usedlist_lock);
}

inline uint64_t size_index_to_size(uint16_t s_idx)
{
    return ((uint64_t)DYN_MIN_DIR_SIZE) << s_idx;
}

inline static int check_mergeable(uint16_t state, uint16_t prev_state, uint16_t sharer, uint16_t prev_sharer)
{
    if ((state != CACHELINE_IDLE) && (state != CACHELINE_SHARED) && (state != CACHELINE_MODIFIED))
        return 0;
    if ((prev_state != CACHELINE_IDLE) && (prev_state != CACHELINE_SHARED) && (prev_state != CACHELINE_MODIFIED))
        return 0;
    return (state == CACHELINE_IDLE) || (prev_state == CACHELINE_IDLE) || ((state == CACHELINE_SHARED) && (prev_state == CACHELINE_SHARED)) || (sharer == prev_sharer);
}
