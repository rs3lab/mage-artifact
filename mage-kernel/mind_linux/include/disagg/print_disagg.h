
#ifndef __PRINT_DISAGGREGATION_H__
#define __PRINT_DISAGGREGATION_H__

#ifdef CONFIG_COMPUTE_NODE
// #define PRINT_CACHE_COHERENCE
// #define PRINT_RDMA_TRANSMISSION
// #define PRINT_DUMMY_RDMA
// #define PRINT_PAGE_FAULT
// #define PRINT_CNTHREAD_RECLAIM
// #define PRINT_MAPS
// #define PRINT_LOCKS
#define PRINT_SYSCALLS
// #define PRINT_SWITCH_STATUS
// #define PRINT_FUTEX
#define PRINT_YASH
#define CONFIG_PROFILING_POINTS
// #define MIND_VERIFY_PAGE_CHKSUM  // disabled due to the overhead
#endif

#ifdef PRINT_CACHE_COHERENCE
#define pr_cache(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_cache(...) \
    do                \
    {                 \
    } while (0)
#endif

#ifdef PRINT_RDMA_TRANSMISSION
#define pr_rdma(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_rdma(...) \
    do               \
    {                \
    } while (0)
#endif

#ifdef PRINT_DUMMY_RDMA
#define pr_dummy(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_dummy(...) \
    do               \
    {                \
    } while (0)
#endif

#ifdef PRINT_PAGE_FAULT
#define pr_pgfault(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_pgfault(...) \
    do                  \
    {                   \
    } while (0)
#endif

#ifdef PRINT_CNTHREAD_RECLAIM
#define pr_reclaim(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_reclaim(...) \
    do                  \
    {                   \
    } while (0)
#endif

#ifdef PRINT_YASH
#define pr_yash(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_yash(...) \
    do                  \
    {                   \
    } while (0)

#endif
#ifdef PRINT_MAPS
#define pr_maps(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_maps(...) \
    do                  \
    {                   \
    } while (0)
#endif

#ifdef PRINT_LOCKS
#define pr_locks(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_locks(...) \
    do                  \
    {                   \
    } while (0)
#endif


#ifdef PRINT_SYSCALLS
#define pr_syscall(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_syscall(...) \
    do                  \
    {                   \
    } while (0)
#endif


#ifdef PRINT_FUTEX
#define pr_futex(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_futex(...) \
    do                \
    {                 \
    } while (0)
#endif

#ifndef print_remote_syscall_only
#define print_remote_syscall_only(...) {if (current->is_remote) pr_syscall(__VA_ARGS__);}
// #define print_remote_syscall_only(...) {;}
#endif
#endif
