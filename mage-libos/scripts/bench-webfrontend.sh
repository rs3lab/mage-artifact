#!/bin/bash
source config.sh
source scripts/config-bench.sh

set -euo

echo $MEM_EXTRA_MB

OUT_PATH="$OUT_PATH/web_frontend/${DATE}"
#MEMORIES=(34000 30600 27200 23800 20400 17000 13600 12000)
MEMORIES=(17000)
#SLEEP_TIME=(30 30 30 27 21 10 1 0) #For Fixed Load
SLEEP_TIME=(120 110 100 90 80 70 60 50 40 30 25 20 15 10 5 0)
#SLEEP_TIME=(0)
#SLEEP_TIME=(0)
#MEMORIES=(8500)
PREFETCHERS=(no)
#PREFETCHERS=(no)

FULL_CLEAN=n

if [[ "${FULL_CLEAN}" = "y" ]]; then
    ./clean.sh
fi

./clean-app.sh
./build.sh hashtable

#export MICRO_CPUS=(1 2 4 8 16 24 28 32 40 48 56)
export MICRO_CPUS=(48)
#export MICRO_CPUS=(24)

./remote.sh down
./remote.sh clean
./remote.sh build

TRIES=(1)
TLB_FLUSH_MODE=patr
LRU_MODE=static_lru

mkdir -p $OUT_PATH
for TRY in ${TRIES[@]}; do
    #for i in "${!MEMORIES[@]}"; do
    for i in "${!SLEEP_TIME[@]}"; do
    M=${MEMORIES[0]}
    ST=${SLEEP_TIME[$i]}
    echo $M
    echo $ST
        for P in ${PREFETCHERS[@]}; do
            for C in ${MICRO_CPUS[@]}; do
                MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep | awk '{ print $2 }' )
                echo $MYPID
                if [[ -n $MYPID ]]; then
                    kill -9 $MYPID
                fi
                
                sed -i "s/kNumMutatorThreads = .*/kNumMutatorThreads = $C;/" apps/hashtable/web_frontend.cpp
                sed -i "s/kThreadSleepTime = .*/kThreadSleepTime = $ST;/" apps/hashtable/web_frontend.cpp
                ./clean-app.sh && ./build.sh hashtable

                ./remote.sh down
                sleep 2
                ./remote.sh up
                sleep 2

                FILE_OUT="${OUT_PATH}/web_frontend-percent-nosync-pipelined-4-selective-no-l2-$TLB_FLUSH_MODE-$LRU_MODE-$M-$P-$C-$TRY-$ST.txt"
                echo ${FILE_OUT}
                install_timeout 600 qemu-system-x86_64
                sleep 1
                RDMA_MODE=selective NO_GLOBAL_L2=y MAX_RECLAIM_THREADS=4 ASYNC_RECLAIM=pipelined SYNC_RECLAIM=no TLB_FLUSH_MODE=$TLB_FLUSH_MODE LRU_MODE=$LRU_MODE MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P ./run.sh /web_frontend | tee ${FILE_OUT} &
                #MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P ./run.sh /web_frontend | tee ${FILE_OUT} &
                sleep 3
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
./remote.sh down
MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep | awk '{ print $2 }' )
echo $MYPID
if [[ -n $MYPID ]]; then
    kill -9 $MYPID
fi
