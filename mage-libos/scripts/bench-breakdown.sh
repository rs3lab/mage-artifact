#!/bin/bash
source config.sh
source scripts/config-bench.sh

recover
set -euo

echo $MEM_EXTRA_MB

OUT_PATH="$OUT_PATH/gapbs/${DATE}"
MEMORIES=(22000 19800 17600 15400 13200 11000 8800 6600 4400 2200) #kron
GRAPH_TRIAL=3

declare -A ALGO_PARAMS
#ALGO_PARAMS[pr]=" -f /mnt/twitter.sg -i1000 -t1e-4"
ALGO_PARAMS[pr]=" -f /mnt/kron.sg -i1000 -t1e-4"
ALGO_PARAMS[bc]=" -f /mnt/twitter.sg -i1 "

ENABLE_PARSE=n

FULL_CLEAN=n

if [[ "${FULL_CLEAN}" = "y" ]]; then
    ./clean.sh
fi

./clean-app.sh
./build.sh gapbs

./remote.sh down
./remote.sh clean
./remote.sh build

mkdir -p $OUT_PATH

export OMP_CPUS=(48)

function prepare_batch(){
    BATCH=$1
    sed -i "s/constexpr size_t current_max_evict = [0-9]\+;/constexpr size_t current_max_evict = ${BATCH};/g" ${ROOT_PATH}/dilos/include/ddc/remote.hh
    ./clean-app.sh
    ./build.sh gapbs
}

function prepare_no_l2(){
    sed -i "s/static constexpr size_t max = [0-9]\+;/static constexpr size_t max = 64;/g" ${ROOT_PATH}/dilos/core/mempool.cc
    ./clean-app.sh
    ./build.sh gapbs
}

function unprepare_no_l2(){
    sed -i "s/static constexpr size_t max = [0-9]\+;/static constexpr size_t max = 512;/g" ${ROOT_PATH}/dilos/core/mempool.cc
    ./clean-app.sh
    ./build.sh gapbs
}

function prepare_no_lru(){
    sed -i "s/constexpr size_t max_queues = [0-9]\+;/constexpr size_t max_queues = 1;/g" ${ROOT_PATH}/dilos/include/ddc/page.hh
    ./clean-app.sh
    ./build.sh gapbs
}

function prepare_sync(){
    sed -i "s/constexpr size_t max_tokens = 64;/constexpr size_t max_tokens = 16;/g" ${ROOT_PATH}/dilos/core-ddc/tlb.hh
    sed -i "s/constexpr size_t current_max_evict = [0-9]\+;/constexpr size_t current_max_evict = 64;/g" ${ROOT_PATH}/dilos/include/ddc/remote.hh
    ./clean-app.sh
    ./build.sh gapbs
}

function unprepare_sync(){
    sed -i "s/constexpr size_t max_tokens = 16;/constexpr size_t max_tokens = 64;/g" ${ROOT_PATH}/dilos/core-ddc/tlb.hh
    sed -i "s/constexpr size_t current_max_evict = [0-9]\+;/constexpr size_t current_max_evict = 256;/g" ${ROOT_PATH}/dilos/include/ddc/remote.hh
    ./clean-app.sh
    ./build.sh gapbs

}

function unprepare_no_lru(){
    sed -i "s/constexpr size_t max_queues = [0-9]\+;/constexpr size_t max_queues = 4;/g" ${ROOT_PATH}/dilos/include/ddc/page.hh
    ./clean-app.sh
    ./build.sh gapbs

}

function run_iteration_async() {
    local A=$1
    local TRY=$2
    local P=$3
    local L=$4
    local T=$5
    local M=$6
    local C=$7
    local GL=$8
    local SUFFIX=$9


    FILE_OUT="${OUT_PATH}/gapbs-kron-nosync-pipelined-4-selective-${BATCH}-$T-$L-$A-$M-$P-$C-$TRY-${SUFFIX}.txt"
    run_async() {
        MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep | grep -v scripts | awk '{ print $2 }' )
        echo $MYPID
        if [[ -n $MYPID ]] && kill -0 "$MYPID" 2>/dev/null; then
            sudo kill -9 "$MYPID"
        fi

        ./remote.sh down
        sleep 2
        ./remote.sh up
        sleep 2

        echo ${FILE_OUT}
        install_timeout 300 qemu-system-x86_64
        sleep 1
        D=$((C-1))
        if [[ $GL == "yes" ]]; then
            NO_GLOBAL_L2=y RDMA_MODE=selective MAX_RECLAIM_THREADS=4 ASYNC_RECLAIM=pipelined SYNC_RECLAIM=no TLB_FLUSH_MODE=${T} LRU_MODE=${L} MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P DISK=${KRON}.raw ./run.sh --env=GOMP_CPU_AFFINITY=0-$D --env=OMP_NUM_THREADS=$C /${A} ${ALGO_PARAMS[$A]} -n${GRAPH_TRIAL} | tee ${FILE_OUT}
        else
            RDMA_MODE=selective MAX_RECLAIM_THREADS=4 ASYNC_RECLAIM=pipelined SYNC_RECLAIM=no TLB_FLUSH_MODE=${T} LRU_MODE=${L} MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P DISK=${KRON}.raw ./run.sh --env=GOMP_CPU_AFFINITY=0-$D --env=OMP_NUM_THREADS=$C /${A} ${ALGO_PARAMS[$A]} -n${GRAPH_TRIAL} | tee ${FILE_OUT}
        fi
        stop_timeout
    }
    run_async
    retry_count=0
    while ! grep -q "Average" "${FILE_OUT}" && [ $retry_count -lt 10 ]; do
        run_async
        retry_count=$((retry_count+1))
    done
}

function run_iteration_sync() {
    local A=$1
    local TRY=$2
    local P=$3
    local L=$4
    local T=$5
    local M=$6
    local C=$7
    local GL=$8
    local SUFFIX=$9

    FILE_OUT="${OUT_PATH}/gapbs-kron-sync-pipelined-1-$T-$L-$A-$M-$P-$C-$TRY-${SUFFIX}.txt"
    run_sync(){
        MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep | awk '{ print $2 }' )
        echo $MYPID
        if [[ -n $MYPID ]] && kill -0 "$MYPID" 2>/dev/null; then
            sudo kill -9 "$MYPID"
        fi

        ./remote.sh down
        sleep 2
        ./remote.sh up
        sleep 2

        echo ${FILE_OUT}
        install_timeout 300 qemu-system-x86_64
        sleep 1
        D=$((C-1))
        MAX_RECLAIM_THREADS=1 SYNC_RECLAIM=normal  TLB_FLUSH_MODE=${T}  LRU_MODE=${L} MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P DISK=${KRON}.raw ./run.sh --env=GOMP_CPU_AFFINITY=0-$D --env=OMP_NUM_THREADS=$C /${A} ${ALGO_PARAMS[$A]} -n${GRAPH_TRIAL} | tee ${FILE_OUT}
        stop_timeout
    }
    run_sync
    retry_count=0
    while ! grep -q "Average" "${FILE_OUT}" && [ $retry_count -lt 10 ]; do
        run_sync
        retry_count=$((retry_count+1))
    done
}

prepare_batch 256
for M in ${MEMORIES[@]}; do
    run_iteration_async pr 1 no static_fifo batch $M 48 no all
done

prepare_no_l2
for M in ${MEMORIES[@]}; do
    run_iteration_async pr 1 no static_fifo batch $M 48 yes nol2
done

prepare_no_lru
for M in ${MEMORIES[@]}; do
    run_iteration_async pr 1 no static_lru batch $M 48 yes nolru
done

prepare_sync
for M in ${MEMORIES[@]}; do
    run_iteration_sync pr 1 no static_lru batch $M 48 yes nopipelined
done

unprepare_sync
unprepare_no_lru
unprepare_no_l2


./remote.sh down
MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep | awk '{ print $2 }' )
echo $MYPID
if [[ -n $MYPID ]]; then
    kill -9 $MYPID
fi
sed -i "s/constexpr size_t current_max_evict = [0-9]\+;/constexpr size_t current_max_evict = 256;/g" ${ROOT_PATH}/dilos/include/ddc/remote.hh

echo "Result in: "
echo ${OUT_PATH}
if [[ "${ENABLE_PARSE}" == "y" ]]; then
    ./scripts/parse-breakdown-mage-libos.sh $OUT_PATH
else
    echo "Skipping parsing."
fi