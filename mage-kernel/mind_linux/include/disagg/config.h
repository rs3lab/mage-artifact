#ifndef __DISAGG_PROGRAM_CONFIG_H__
#define __DISAGG_PROGRAM_CONFIG_H__

// Configuration file for test

// #define TEST_PROGRAM_NAME "test_mltthrd"
// #define TEST_PROGRAM_DIGIT 12
// #define TEST_PROGRAM_NAME "gapbs_pr"
// #define TEST_PROGRAM_DIGIT 8
//#define TEST_PROGRAM_NAME "wrmem"
//#define TEST_PROGRAM_DIGIT 5
//#define TEST_PROGRAM_NAME "XSBench"
//#define TEST_PROGRAM_DIGIT 7
//#define TEST_PROGRAM_NAME "web_frontend"
//#define TEST_PROGRAM_DIGIT 12
#define TEST_PROGRAM_NAME "memcached"
#define TEST_PROGRAM_DIGIT 9

#define LAUNCHER_PROGRAM_NAME "launcher_thread"
#define EXAMPLE_PROGRAM_NAME "exmp_mltthrd"

// NOTE: switch control plane may have different limit for total number of processes in a cluster
#define MAX_PROCESS_BUCKET_SIZE 1024   
#define MAX_PROCESS_BUCKET_MASK (MAX_PROCESS_BUCKET_SIZE-1)

#define CNPAGE_LOCK_TABLE_BITS 9
#define CNPAGE_LOCK_TABLE_SIZE (1 << CNPAGE_LOCK_TABLE_BITS)

#define REMOTE_THREAD_SLEEP_INIT_IN_SECOND 1
#define TEST_INIT_ALLOC_SIZE (4 * 1024 * 1024 * 1024UL)  // 4 GB
#define TEST_MACRO_ALLOC_SIZE (8 * 1024 * 1024 * 1024UL)  // 8 GB
#define TEST_SUB_REGION_ALLOC_SIZE (1 * 1024 * 1024 * 1024UL)  // 1 GB
#define TEST_META_ALLOC_SIZE (32 * 1024 * 1024UL)  // 32 MB
#define TEST_ALLOC_FLAG 0xfe
#define TEST_ALLOC_FILE_FLAG 0xfd
#define TEST_PROGRAM_TGID 62233
#define RESET_DIR_WHEN_FAIL 0
#define TEST_DEBUG_SIZE_LIMIT (256L * 1024 * 1024 * 1024)
#define NFS_SERVER_BASE "/shared_libraries_nfs"

// Futext relatd values
#define TEST_MAX_FUTEX 2048
#define TEST_PAGE_SIZE 4096UL
#define TEST_MAX_LOCKS 8192
#define TEST_ALLOC_ADDR 0x7f0000000000
#define TEST_MEM_ACC_ADDR TEST_ALLOC_ADDR
#define TEST_LOCK_ADDR (TEST_MEM_ACC_ADDR + TEST_PAGE_SIZE * TEST_MAX_LOCKS)
#define TEST_TLS_ADDR (TEST_LOCK_ADDR + TEST_PAGE_SIZE * TEST_MAX_LOCKS)

// Kernel shared memory
#define DISAGG_KERN_SHMEM_SIZE_IN_PAGE (32768)   // 128 MB
#define DISAGG_KERN_SHMEM_SIZE (DISAGG_KERN_SHMEM_SIZE_IN_PAGE * PAGE_SIZE)

// Process management
#define MIND_EXIT_RETRY_IN_SEC 5

#ifndef __TEST__
// Test VMA inditifier: body is inside mmap_disagg.c
int TEST_is_target_vma(unsigned long vm_start, unsigned long vm_end);
int TEST_is_sub_target_vma(unsigned long vm_start, unsigned long vm_end);
int TEST_is_meta_vma(unsigned long vm_start, unsigned long vm_end);
int TEST_is_test_vma(unsigned long vm_start, unsigned long vm_end);
// Thread counter for test program
void init_test_program_thread_cnt(void);
int *get_test_program_thread_cnt(int tgid);
int increment_test_program_thread_cnt(int tgid);
int decrement_test_program_thread_cnt(int tgid);

#endif
#endif
