/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef PAGEALLOC_HH_
#define PAGEALLOC_HH_

#include <stddef.h>

namespace memory {

void* alloc_page();
void free_page(void* page);
void free_page_to_reclaim_buf(void* page);
void clear_reclaim_buf();
// If smp_allocator is enabled and we don't have waiter to refill l2, then 
// free to local l1, otherwise free to free_page_ranges.
void free_page_to_range(void* page);
void* alloc_huge_page(size_t bytes);
void free_huge_page(void *page, size_t bytes);

}

#endif /* PAGEALLOC_HH_ */
