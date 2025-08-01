#!/bin/bash
source config.sh
source scripts/config-bench.sh

recover
set -euo

echo $MEM_EXTRA_MB

OUT_PATH="$OUT_PATH/xsbench/${DATE}"
MEMORIES=(18000 15000 13500 12000 10500 9000 7500 6000 4500 3000 1500)
#MEMORIES=(${FULL_MB})
PREFETCHERS=(no)
#LRU_MODES=(static_fifo static_lru)
#TLB_FLUSH_MODES=(patr batch)
LRU_MODES=(static_fifo)
TLB_FLUSH_MODES=(patr)
#TLB_FLUSH_MODES=(batch)

ENABLE_PARSE=y

FULL_CLEAN=n

if [[ "${FULL_CLEAN}" = "y" ]]; then
    ./clean.sh
fi

./clean-app.sh
./build.sh xsbench

#export OMP_CPUS=(1 2 4 8 16 24 28 32 40 48 56)
export OMP_CPUS=(48)
#export OMP_CPUS=(48)
TRIES=(1)

./remote.sh down
./remote.sh clean
./remote.sh build

mkdir -p $OUT_PATH
for TRY in ${TRIES[@]}; do
    for P in ${PREFETCHERS[@]}; do
        for L in ${LRU_MODES[@]}; do
            for T in ${TLB_FLUSH_MODES[@]}; do
                for M in ${MEMORIES[@]}; do
                    for C in ${OMP_CPUS[@]}; do
                        MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep |grep -v scripts | awk '{ print $2 }' )
                        echo $MYPID
                        if [[ -n $MYPID ]]; then
                            sudo kill -9 $MYPID
                        fi

                        ./remote.sh down
                        sleep 2
                        ./remote.sh up
                        sleep 2

                        FILE_OUT="${OUT_PATH}/xsbench-nosync-pipelined-4-selective-$T-$L-$M-$P-$C-$TRY.txt"
                        echo ${FILE_OUT}
                        install_timeout 3000 qemu-system-x86_64
                        sleep 1
                        D=$((C-1))
                        RDMA_MODE=selective MAX_RECLAIM_THREADS=4 ASYNC_RECLAIM=pipelined SYNC_RECLAIM=no  TLB_FLUSH_MODE=${T}  LRU_MODE=${L} MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P DISK=${KRON}.raw ./run.sh --env=GOMP_CPU_AFFINITY=0-$D --env=OMP_NUM_THREADS=$C /XSBench -t $C -m history -s XL -l 34 -p 5000000 -G unionized -g 30000 | tee ${FILE_OUT}
                        #MAX_RECLAIM_THREADS=4 SYNC_RECLAIM=normal  TLB_FLUSH_MODE=${T}  LRU_MODE=${L} MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P DISK=${TWITTER}.raw ./run.sh --env=GOMP_CPU_AFFINITY=0-$D --env=OMP_NUM_THREADS=$C /${A} ${ALGO_PARAMS[$A]} -n${GRAPH_TRIAL} | tee ${FILE_OUT}
                        stop_timeout
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

echo "Result in: "
echo ${OUT_PATH}
if [[ "${ENABLE_PARSE}" == "y" ]]; then
    ./scripts/parse-xsbench-mage-libos.sh $OUT_PATH
else
    echo "Skipping parsing."
fi