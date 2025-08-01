#!/bin/bash
source config.sh
source scripts/config-bench.sh

recover
set -euo

echo $MEM_EXTRA_MB

OUT_PATH="$OUT_PATH/memcached/${DATE}"
MEMCACHED_PATH="$ROOT_PATH/dilos/apps/memcached"
PREFETCHERS=(no)
LRU_MODES=(static_fifo)
TLB_FLUSH_MODES=(batch)

FULL_CLEAN=n

if [[ "${FULL_CLEAN}" = "y" ]]; then
    ./clean.sh
fi

sed -i "s/#define SLEEP_TIME .*/#define SLEEP_TIME 0/" $MEMCACHED_PATH/src/test_config.h
./clean-app.sh
./build.sh memcached

CORES=24
MEMORIES=(20480 18432 16384 14336 12288 10240 8192 8000)
export SLEEP_TIMES=(200 100 80 50 20 5 0 0)

./remote.sh down
./remote.sh clean
./remote.sh build

TRIES=(1)

ENABLE_PARSE=y

mkdir -p $OUT_PATH
for TRY in ${TRIES[@]}; do
    for P in ${PREFETCHERS[@]}; do
        for L in ${LRU_MODES[@]}; do
            for T in ${TLB_FLUSH_MODES[@]}; do
                for i in "${!MEMORIES[@]}"; do
                    M=${MEMORIES[$i]}
                    S=${SLEEP_TIMES[$i]}
                        
                    sed -i "s/#define SLEEP_TIME .*/#define SLEEP_TIME $S/" $MEMCACHED_PATH/src/test_config.h
                    ./clean-app.sh
                    ./build.sh memcached


                    MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep | awk '{ print $2 }' )
                    echo $MYPID
                    if [[ -n $MYPID ]]; then
                        kill -9 $MYPID
                    fi

                    ./remote.sh down
                    sleep 2
                    ./remote.sh up
                    sleep 2

                    FILE_OUT="${OUT_PATH}/memcached-nosync-pipelined-4-selective-$T-$L-$M-$P-$CORES-$S-$TRY.txt"
                    echo ${FILE_OUT}
                    install_timeout 3000 qemu-system-x86_64
                    sleep 1
                    RDMA_MODE=selective MAX_RECLAIM_THREADS=4 ASYNC_RECLAIM=pipelined SYNC_RECLAIM=no  TLB_FLUSH_MODE=${T}  LRU_MODE=${L} MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P ./run.sh /memcached -u root -t ${CORES} -m20000 -v | tee ${FILE_OUT} &
                    sleep 5
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
    ./scripts/parse-memcached-a-mage-libos.sh $OUT_PATH
else
    echo "Skipping parse"
fi
