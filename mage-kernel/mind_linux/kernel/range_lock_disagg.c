#include <disagg/range_lock_disagg.h>

// Relevant methods are in `range_lock_disagg.h`

// DEFINE_PP(RLT_total);
// DEFINE_PP(RLT_lock_tree);

struct cnpage_lock_bucket cnpage_lock_table[CNPAGE_LOCK_TABLE_SIZE];

void init_cnpage_range_locks(void) {
    size_t i;
    for (i = 0; i < CNPAGE_LOCK_TABLE_SIZE; i++)
    {
        struct cnpage_lock_bucket *bucket = &cnpage_lock_table[i];
        cnpage_lock_tree_init(&bucket->range_lock_root);
    }
}
