#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef uint64_t (*ddc_eviction_mask_f)(uintptr_t vaddr);

void ddc_register_eviction_mask(ddc_eviction_mask_f f);

#ifdef __cplusplus
}
typedef struct slice_pages_active_stat{
    uint32_t n_slices; /* Times to slice pages from active list */
    uint32_t n_sliced_pages; /* The total number of pages sliced */
    uint32_t n_pages_skip; /* The pages skipped */
    uint32_t n_pages_finish; /* The pages finished. Can be reclaimed */
    uint32_t n_pages_tlb; /* The pages waiting for TLB */
    uint32_t n_pages_evict; /* The pages waiting for RDMA write */
    uint32_t n_pages_failed; /* The pages failed to update */
} slice_pages_active_stat_t;
#endif