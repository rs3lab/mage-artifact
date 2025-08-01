#ifndef __KERNEL_SHARED_MEMROY_H__
#define __KERNEL_SHARED_MEMROY_H__

#include "memory_management.h"

unsigned long kshmem_alloc(int nid, unsigned long addr, unsigned long size);
int kshmem_free(unsigned long va);
void kshmem_set_va_start(u64 va_start, int nid);
u64 kshmem_get_va_start(void);

// Request handler
int handle_kshmem_alloc(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_kshmem_base_addr(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_kshmem_alloc_va(struct mem_header *hdr, void *payload, struct socket *sk, int id);

#endif
