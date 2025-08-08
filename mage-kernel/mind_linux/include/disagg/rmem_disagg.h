#ifndef __RMEM_DISAGG_H__
#define __RMEM_DISAGG_H__

#include <linux/types.h>

/*
 * This API manages remote memory. It uses a primitive memory allocator to manage the remote
 * address space, and automatically updates the (CN VA) -> (MN VA) address mapping tables (which
 * are in `cnmap_disagg.h`
 */

int rmem_alloc(u16 tgid, u64 va, size_t size);
int rmem_free(u16 tgid, u64 va, size_t size);
int rmem_free_tgid(u16 tgid);
int rmem_free_all(void);

#endif /* __RMEM_DISAGG_H__ */
