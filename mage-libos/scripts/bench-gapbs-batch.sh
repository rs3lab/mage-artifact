#!/bin/bash
source config.sh
source scripts/config-bench.sh

recover
set -euo

echo $MEM_EXTRA_MB

OUT_PATH="$OUT_PATH/gapbs/${DATE}"
#MEMORIES=(13000 9750 6500 3250)
#MEMORIES=(13000 11700 10400 9100 7800 6500 5200 3900 2600) #twitter
MEMORIES=(22000 19800 17600 15400 13200 11000 8800 6600 4400 2200) #kron
#MEMORIES=(8800 6600 4400 2200) #kron
#MEMORIES=(14000 12600 11200 9800 8400 7000 5600 4200 2800 1400)
#MEMORIES=(${FULL_MB})
PREFETCHERS=(no)
ALGO=(pr)
GRAPH_TRIAL=3
#LRU_MODES=(static_fifo static_lru)
#TLB_FLUSH_MODES=(patr batch)
#LRU_MODES=(static_fifo)
LRU_MODES=(static_fifo)
#TLB_FLUSH_MODES=(patr)
TLB_FLUSH_MODES=(batch)
BATCH_SIZE=(256 128 64)

declare -A ALGO_PARAMS
#ALGO_PARAMS[pr]=" -f /mnt/twitter.sg -i1000 -t1e-4"
ALGO_PARAMS[pr]=" -f /mnt/kron.sg -i1000 -t1e-4"
ALGO_PARAMS[bc]=" -f /mnt/twitter.sg -i1 "

ENABLE_PARSE=y

FULL_CLEAN=n

if [[ "${FULL_CLEAN}" = "y" ]]; then
    ./clean.sh
fi

./clean-app.sh
./build.sh gapbs

#export OMP_CPUS=(1 2 4 8 16 24 28 32 40 48 56)
#export OMP_CPUS=(4)
export OMP_CPUS=(48)

./remote.sh down
./remote.sh clean
./remote.sh build

mkdir -p $OUT_PATH
for BATCH in ${BATCH_SIZE[@]}; do
    sed -i "s/constexpr size_t current_max_evict = [0-9]\+;/constexpr size_t current_max_evict = ${BATCH};/g" ${ROOT_PATH}/dilos/include/ddc/remote.hh
    ./clean-app.sh
    ./build.sh gapbs
    for A in ${ALGO[@]}; do
        for TRY in ${TRIES[@]}; do
            for P in ${PREFETCHERS[@]}; do
                for L in ${LRU_MODES[@]}; do
                    for T in ${TLB_FLUSH_MODES[@]}; do
                        for M in ${MEMORIES[@]}; do
                            for C in ${OMP_CPUS[@]}; do
                                FILE_OUT="${OUT_PATH}/gapbs-kron-nosync-pipelined-4-selective-${BATCH}-$T-$L-$A-$M-$P-$C-$TRY.txt"
                                run_async() {
                                    MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep | awk '{ print $2 }' )
                                    echo $MYPID
                                    if [[ -n $MYPID ]]; then
                                        kill -9 $MYPID
                                    fi

                                    ./remote.sh down
                                    sleep 2
                                    ./remote.sh up
                                    sleep 2

                                    #FILE_OUT="${OUT_PATH}/gapbs-sync-pipelined-4-$T-$L-$A-$M-$P-$C-$TRY.txt"
                                    echo ${FILE_OUT}
                                    install_timeout 3000 qemu-system-x86_64
                                    sleep 1
                                    D=$((C-1))
                                    RDMA_MODE=selective MAX_RECLAIM_THREADS=4 ASYNC_RECLAIM=pipelined SYNC_RECLAIM=no TLB_FLUSH_MODE=${T} LRU_MODE=${L} MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P DISK=${KRON}.raw ./run.sh --env=GOMP_CPU_AFFINITY=0-$D --env=OMP_NUM_THREADS=$C /${A} ${ALGO_PARAMS[$A]} -n${GRAPH_TRIAL} | tee ${FILE_OUT}
                                    #MAX_RECLAIM_THREADS=4 SYNC_RECLAIM=normal  TLB_FLUSH_MODE=${T}  LRU_MODE=${L} MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P DISK=${TWITTER}.raw ./run.sh --env=GOMP_CPU_AFFINITY=0-$D --env=OMP_NUM_THREADS=$C /${A} ${ALGO_PARAMS[$A]} -n${GRAPH_TRIAL} | tee ${FILE_OUT}
                                    stop_timeout
                                }
                                retry_count=0
                                run_async
                                while ! grep -q "Average" "${FILE_OUT}" && [ $retry_count -lt 10 ]; do
                                    run_async
                                    retry_count=$((retry_count+1))
                                done
                            done
                        done
                    done
                done
            done
        done
    done
done

LRU_MODES=(static_lru)
TLB_FLUSH_MODES=(batch)
for A in ${ALGO[@]}; do
    for TRY in ${TRIES[@]}; do
        for P in ${PREFETCHERS[@]}; do
            for L in ${LRU_MODES[@]}; do
                for T in ${TLB_FLUSH_MODES[@]}; do
                    for M in ${MEMORIES[@]}; do
                        for C in ${OMP_CPUS[@]}; do
                            FILE_OUT="${OUT_PATH}/gapbs-kron-sync-pipelined-4-$T-$L-$A-$M-$P-$C-$TRY.txt"
                            run_sync(){
                                MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep | awk '{ print $2 }' )
                                echo $MYPID
                                if [[ -n $MYPID ]]; then
                                    kill -9 $MYPID
                                fi

                                ./remote.sh down
                                sleep 2
                                ./remote.sh up
                                sleep 2

                                echo ${FILE_OUT}
                                install_timeout 3000 qemu-system-x86_64
                                sleep 1
                                D=$((C-1))
                                MAX_RECLAIM_THREADS=4 SYNC_RECLAIM=normal  TLB_FLUSH_MODE=${T}  LRU_MODE=${L} MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P DISK=${KRON}.raw ./run.sh --env=GOMP_CPU_AFFINITY=0-$D --env=OMP_NUM_THREADS=$C /${A} ${ALGO_PARAMS[$A]} -n${GRAPH_TRIAL} | tee ${FILE_OUT}
                                stop_timeout
                            }
                            run_sync
                            retry_count=0
                            while ! grep -q "Average" "${FILE_OUT}" && [ $retry_count -lt 10 ]; do
                                run_sync
                                retry_count=$((retry_count+1))
                            done
                        done
                    done
                done
            done
        done
    done
done
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
    ./scripts/parse-gapbs-batch-mage-libos.sh $OUT_PATH
else
    echo "Skipping parsing."
fi