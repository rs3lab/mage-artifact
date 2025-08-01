#!/bin/bash
source config.sh
source scripts/config-bench.sh

recover
set -euo

echo $MEM_EXTRA_MB

OUT_PATH="$OUT_PATH/microbench/${DATE}"
MEMORIES=(7168)
PREFETCHERS=(no)
STRIDES=(1)

FULL_CLEAN=n

if [[ "${FULL_CLEAN}" = "y" ]]; then
    ./clean.sh
fi

./clean-app.sh
./build.sh microbench

export MICRO_CPUS=(1 2 4 8 16 24 28 32 40 48)
#export MICRO_CPUS=(24)

./remote.sh down
./remote.sh clean
./remote.sh build

TRIES=(1)

ENABLE_PARSE=y

mkdir -p $OUT_PATH
for TRY in ${TRIES[@]}; do
    for M in ${MEMORIES[@]}; do
        for S in ${STRIDES[@]}; do
            for P in ${PREFETCHERS[@]}; do
                for C in ${MICRO_CPUS[@]}; do
                    MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep | awk '{ print $2 }' )
                    echo $MYPID
                    if [[ -n $MYPID ]]; then
                        kill -9 $MYPID
                    fi

                    ./remote.sh down
                    sleep 2
                    ./remote.sh up
                    sleep 2

                    #FILE_OUT="${OUT_PATH}/microbench-$M-$P-$C-$TRY-$S.txt"
                    FILE_OUT="${OUT_PATH}/microbench-selective-4-pipelined-nosync-patr-static-fifo-$M-$P-$C-$TRY-$S.txt"
                    echo ${FILE_OUT}
                    install_timeout 3000 qemu-system-x86_64
                    sleep 1
                    # -w: Write; no -w: Read
                    #MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P ./run.sh /page_fault -t $C -d -s $S -w -i 5 | tee ${FILE_OUT} &
                    MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P RDMA_MODE=selective MAX_RECLAIM_THREADS=4 ASYNC_RECLAIM=pipelined SYNC_RECLAIM=no  TLB_FLUSH_MODE=patr  LRU_MODE=static_fifo ./run.sh /page_fault -t $C -d -s $S | tee ${FILE_OUT} &
                    sleep 1
                    sudo ./scripts/pin-vcpu.py
                    MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep | awk '{ print $2 }' )
                    while [[ -n $MYPID ]]; do
                        sleep 1
                        MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep | awk '{ print $2 }' )
                    done
                    stop_timeout
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
    ./scripts/parse-microbench-mage-libos.sh $OUT_PATH
else
    echo "Skipping parse"
fi