#!/bin/bash
source config.sh
source scripts/config-bench.sh

set -euo

echo $MEM_EXTRA_MB

OUT_PATH="$OUT_PATH/minorfault/${DATE}"
MEMORIES=(20480)
PREFETCHERS=(no)
#PREFETCHERS=(no)
STRIDES=(1)
#STRIDES=(1)

FULL_CLEAN=n

if [[ "${FULL_CLEAN}" = "y" ]]; then
    ./clean.sh
fi

./clean-app.sh
./build.sh microbench

export MICRO_CPUS=(1 2 4 8 16 24 28 32 40 48 56)
#export MICRO_CPUS=(24)

./remote.sh down
./remote.sh clean
./remote.sh build

TRIES=(1)

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

                    FILE_OUT="${OUT_PATH}/microbench-$M-$P-$C-$TRY-$S.txt"
                    echo ${FILE_OUT}
                    install_timeout 3000 qemu-system-x86_64
                    sleep 1
                    MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P ./run.sh /page_fault -t $C -d -w -s $S -m | tee ${FILE_OUT} &
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
