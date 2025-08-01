### ROOT Path

ROOT_PATH=$(dirname "$(realpath "${BASH_SOURCE[0]}")")
cd $ROOT_PATH || exit

### Compute Node Configuration ###

BUILD_TYPE=RelWithDebInfo
NO_SG=${NO_SG:=y}

# Frequency Configuration
FREQ=2600000

# Network Configuration

ETH_IF=ens259f1
IP=19.18.1.2
SUBNET=255.255.255.0
PREFIX=24
GW=19.18.1.3
NAMESERVER=8.8.8.8
TAP_NAME=pusnow_tap0
RDMA_IF=ibs785f0
RDMA_IP=18.18.1.1

# CPU Configuration

AUTO_POWEROFF=y
HYPERVISOR=qemu_microvm
NODE=0,1
NODE_CPUS=0-55
CPUS=${CPUS:=56}
MEMORY=${MEMORY:=7G}
FS=rofs
PCI=device
PREFETCHER=${PREFETCHER}
SYNC_RECLAIM=${SYNC_RECLAIM}
MAX_RECLAIM_THREADS=${MAX_RECLAIM_THREADS}
NO_GLOBAL_L2=${NO_GLOBAL_L2}
LRU_MODE=${LRU_MODE}
TLB_FLUSH_MODE=${TLB_FLUSH_MODE}
NO_SCAN_FLUSH=${NO_SCAN_FLUSH}

#TRACE=ddc_prefetcher_readahead_event
#TRACE=ddc_async_tlb_coop_check
#TRACE=ddc_async_pipelined_*
#TRACE=l2_refill_wait_for_page,l2_refill_ret
#TRACE=ddc_async_evict_slice,ddc_async_finalize,ddc_async_clean_slice*,ddc_async_tlb*
#TRACE=ddc_sync_reclaim,ddc_sync_reclaimed_pages,ddc_async_reclaimed_pages
#TRACE=l2_refill_wait,l2_refill_all,l2_refill_locking
#TRACE=concurrent_on_allocator,buddy_allocator*
#TRACE=ddc_tlb_flush_cooperative,ddc_tlb_flush
#TRACE=ddc_evict_accessed,ddc_clean_slice_update_fail
#TRACE=ddc_inactive_is_low
#TRACE=ddc_slice_page_active,ddc_slice_page_active_ret,ddc_push_page_active,ddc_push_page_active_ret
#TRACE=ddc_threads
#TRACE=ddc_dynamic_stat
#TRACE=ddc_prefetch_majority*,ddc_mmu_prefetch_remote
#TRACE=mutex_lock_before_wait,mutex_lock_before_wait_ret
#TRACE=do_sync_reclaim*
#TRACE=ddc_slice_pages_active_number
#TRACE=ddc_clean_slice_pages_active*
#TRACE=ddc_poll_until_one_locked*
#TRACE=ddc_evict_slice_pages_clean*
#TRACE=ddc_evict_push_pages_active*
#TRACE=do_sync_reclaim*,ddc_evict_request_memory*,ddc_finalize*,ddc_push_page*
#TRACE=ddc_clean_slice*
#TRACE=tlb_flush_all_inside_lock*
#TRACE=ddc_dirty_handler_bottom_half*
#TRACE=ddc_accessed_handler_bottom_half*,ddc_tlb_push_inside_lock*
#TRACE=ddc_accessed_handler_bottom_half*,ddc_dirty_handler_bottom_half*,ddc_clean_slice*
#TRACE=ddc_accessed_handler_bottom_half*,ddc_tlb_push_inside_lock*
#TRACE=ddc_dirty_handler_bottom_half*,ddc_dirty_handler_inner*
#TRACE=ddc_accessed_handler_bottom_half*,ddc_dirty_handler_bottom_half*,ddc_tlb_flush*
#TRACE=l2_refill*,ddc_evict_slice*,ddc_evict_request_memory*,ddc_clean_slice*,ddc_accessed_handler_bottom_half*,ddc_dirty_handler_bottom_half*,ddc_tlb_flush*
#TRACE=memory_wakeup_waiter,memory_reclaim,l2_free_page_batch,memory_loop,memory_patch,memory_call_wait,l2_unfill,memory_wait,l1_unfill,l1_free_page_local,l1_alloc_page_local,l2_alloc_page_batch,l2_concurrent_wake_all,l2_concurrent_wake_one,l2_sync_reclaim_working,concurrent_working,ddc_cleaner_*,ddc_mmu_fault_*,l2_refill_enter,l2_refill_exit,ddc_evict_request_*,ddc_evict_loop,ddc_before_*,ddc_after_*,ddc_eviction_*,ddc_evict_find_candidate,ddc_finalize_*,thread_create,ddc_mmu_polled,ddc_mmu_handle_insert_done,ddc_clean_find_candidate,ddc_clean_*
#pr_*
#mmu_vm_fault_sigsegv,ddc_request_memory,memory_*
#ddc_remote*
#ddc_evict*,ddc_mmu_fault_*
#ddc_mmu_*
#ddc_mmu_fault_*,ddc_evict_*
#ddc_mmu_prefetch_remote_offset,ddc_mmu_prefetch_remote_offset_page,ddc_mmu_polled*
#ddc_evict_\*
#,ddc_mmu_fault_remote_page_done\*,ddc_eviction_\*
AUTO_POWEROFF=${AUTO_POWEROFF:=n}

# Remote Configuration

REMOTE_DRIVER=${REMOTE_DRIVER:=rdma}
REMOTE_LOCAL_SIZE_GB=32

if [[ "$REMOTE_DRIVER" == "local" ]]; then
    MEM_EXTRA_MB=$(expr ${REMOTE_LOCAL_SIZE_GB} \* 1024)
else
    MEM_EXTRA_MB=0
fi

# Allocator Configuration
MIMALLOC=${MIMALLOC:=mimalloc}
#-subpage

# Infiniband Configuration

IB_DEVICE=mlx5_0
IB_PORT=1
GID_IDX=1

### Compute Node Configuration (END) ###

### Memory Node Configuration ###
MS_ETH_IF=ens259f1
MS_IP=19.18.1.1
MS_PORT=12346
MS_PATH="~/dilos-artifact"
MS_NODE=0
MS_RDMA_IF=ibs785f0
MS_RDMA_IP=18.18.1.2
MS_MEMORY_GB=${MS_MEMORY_GB:=80}

### Memory Node Configuration (END) ###

### Benchmark Configuration ###

OUT_PATH=$HOME/benchmark-out-ae/
DATE=$(date +%Y-%m-%d_%H-%M-%S)

TWITTER=/scratch/twitter/twitter.sg
TWITTERU=/scratch/twitter/twitterU.sg
TRIP_DATA_CSV=/scratch/tripdata/all.csv
ENWIKI_UNCOMP=/scratch/enwiki/enwik9.uncompressed
ENWIKI_COMP=/scratch/enwiki/enwik9.compressed
KRON=/scratch/kron/kron.sg

### Benchmark Configuration (END) ###
