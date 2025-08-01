#!/bin/bash
source config.sh

QEMU_PATH=build/qemu

source config.sh

if [[ -n $TRACE ]]; then
    TRACE_ARGS="--trace=${TRACE}"
fi

if [[ -n $NODE ]]; then
    if [[ -n $NODE_CPUS ]]; then
        NUMA_ARG="--numa ${NODE} --numa_cpus ${NODE_CPUS}"
    else
        NUMA_ARG="--numa ${NODE}"
    fi
fi

if [[ -n $VERBOSE ]]; then
    VERBOSE_ARG="--verbose"
fi
if [[ -z $NO_NETWORK ]]; then
    NETWORK_ARGS="-n -b dilosbr --vhost"
fi

if [[ -n $DISK ]]; then
    DISK_HD=hd1
    DISK_BLK=vblk1
    DISK_IMG_OPT="--pass-args=-device --pass-args=virtio-blk-${PCI},id=${DISK_BLK},drive=${DISK_HD},scsi=off --pass-args=-drive --pass-args=file=${DISK},if=none,id=${DISK_HD},cache=none,aio=native "
    DISK_MOUNT_CMD="--mount-fs=rofs,/dev/${DISK_BLK},/mnt"
#    DISK_MOUNT_CMD_RAM="--mount-fs=ramfs,none,/dev/shm"
fi

RUN_ARGS="${VERBOSE_ARG} \
--qemu-path ${QEMU_PATH}/x86_64-softmmu/qemu-system-x86_64 \
-p ${HYPERVISOR} \
-c ${CPUS} \
-m ${MEMORY} \
${NUMA_ARG} \
${NETWORK_ARGS} \
--pass-args=-device --pass-args=virtio-uverbs-${PCI},host=uverbs0 \
${DISK_IMG_OPT} \
${TRACE_ARGS} "

IB_CMD="--ib_device=${IB_DEVICE} --ib_port=${IB_PORT} --ms_ip=${MS_IP} --ms_port=${MS_PORT} --gid_idx=${GID_IDX}"
NETWORK_CMD="--ip=eth0,${IP},${SUBNET} --defaultgw=${GW} --nameserver=${NAMESERVER}"

# PREFETCHER
if [[ -n "${PREFETCHER}" ]]; then
    PREFETCHER_CMD="--prefetcher=${PREFETCHER}"
fi

# SYNC_RECLAIM
if [[ -n "${SYNC_RECLAIM}" ]]; then
    SYNC_RECLAIM_CMD="--sync-reclaim=${SYNC_RECLAIM}"
fi

# ASYNC_RECLAIM
if [[ -n "${ASYNC_RECLAIM}" ]]; then
    ASYNC_RECLAIM_CMD="--async-reclaim=${ASYNC_RECLAIM}"
fi

# MAX ASYNC RECLAIM THREADS
if [[ -n "${MAX_RECLAIM_THREADS}" ]]; then
    RECLAIM_THREADS_CMD="--max-reclaim-threads=${MAX_RECLAIM_THREADS}"
fi

# GLOBAL L2 (queue / list)
if [[ -n "${NO_GLOBAL_L2}" ]]; then
    GLOBAL_L2_CMD="--disable-global-l2"
fi

# LRU MODE
if [[ -n "${LRU_MODE}" ]]; then
    LRU_MODE_CMD="--lru-mode=${LRU_MODE}"
fi

# TLB FLUSH MODE
if [[ -n "${TLB_FLUSH_MODE}" ]]; then
    TLB_FLUSH_MODE_CMD="--tlb-flush-mode=${TLB_FLUSH_MODE}"
fi

# RDMA MODE
if [[ -n "${RDMA_MODE}" ]]; then
    RDMA_MODE_CMD="--rdma-mode=${RDMA_MODE}"
fi

# SCAN FLUSH (TODO: Impl)
if [[ -n "${NO_SCAN_FLUSH}" ]]; then
    SCAN_FLUSH_CMD="--disable-scan-flush"
fi

if [[ "${AUTO_POWEROFF}" = "y" ]]; then
    POWEROFF_CMD="--power-off-on-abort"
else
    POWEROFF_CMD="--noshutdown"
fi

if [[ -n "${PIN}" ]]; then
    PIN_CMD="--cpu_pin=${PIN}"
fi

CMD_PREFIX="${NETWORK_CMD} ${IB_CMD} ${PREFETCHER_CMD} ${DISK_MOUNT_CMD} ${POWEROFF_CMD} ${PIN_CMD} ${SYNC_RECLAIM_CMD} ${ASYNC_RECLAIM_CMD}  ${RECLAIM_THREADS_CMD} ${GLOBAL_L2_CMD} ${LRU_MODE_CMD} ${TLB_FLUSH_MODE_CMD} ${RDMA_MODE_CMD} ${SCAN_FLUSH_CMD} --disable_rofs_cache"

if [[ -n "$*" ]]; then
    CMDLINE="$*"
else
    CMDLINE=$(cat dilos/build/last/cmdline)
fi

echo $RUN_ARGS
echo $CMD_PREFIX
echo $CMDLINE

export QEMU_NBD=${ROOT_PATH}/${QEMU_PATH}/qemu-nbd
export QEMU_IMG=${ROOT_PATH}/${QEMU_PATH}/qemu-img

sudo ./dilos/scripts/run.py ${RUN_ARGS} -e "${CMD_PREFIX} ${CMDLINE}"
