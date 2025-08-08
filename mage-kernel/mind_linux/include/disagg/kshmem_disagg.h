#ifndef __KSHMEM_DISAGGREGATION_H__
#define __KSHMEM_DISAGGREGATION_H__

#include <linux/sched.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define DISAGG_KERN_TGID 0xffff     // first 16 bit of 64 bit kernel VA
#define DISAGG_KERN_PID_NS_ID 0

// #define ENABLE_DISAGG_KERN_SHMEM

struct kshmem_msg_struct
{
    unsigned int serv_id;
    unsigned long size;
} __packed;

struct kshmem_reply_struct
{
    u32 ret;
    u64 addr;
}__packed;

struct kshmem_va_msg_struct
{
    unsigned int serv_id;
    unsigned long addr;
    unsigned long size;
    unsigned int reserved[5];
} __packed;

#define DISAGG_KSHMEM_RETRY_TIME_MS 2500

// == KERN_SHMEM == //
// - interface for kernel side shared memory
void prealloc_kernel_shared_mem(void);
void init_kernel_shared_mem(void);
void *alloc_kshmem(unsigned long size, unsigned int serv_id);
void *alloc_kshmem_va(unsigned long addr, unsigned long size, unsigned int serv_id);
void *get_base_address(unsigned int serv_id);
void free_kshmem(void *alloc_va);
int is_kshmem_available(void);
unsigned long kshmem_get_local_start_va(void);
int is_kshmem_address(unsigned long addr);

#define DISAGG_KSHMEM_SERV_FUTEX_ID 1
#define DISAGG_KSHMEM_SERV_FS_ID 10
#define DISAGG_KSHMEM_SERV_DEBUG_ID 99
#define DISAGG_KSHMEM_SERV_MAX_NUM 128

#endif //__KSHMEM_DISAGGREGATION_H__
