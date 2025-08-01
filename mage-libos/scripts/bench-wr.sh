#!/bin/bash
source config.sh
source scripts/config-bench.sh

set -euo

echo $MEM_EXTRA_MB

OUT_PATH="$OUT_PATH/wrmem/${DATE}"
#MEMORIES=(30000 22500 15000 7500)
MEMORIES=(30000 27000 24000 21000 18000 15000 12000 9000 6000 3000)
#MEMORIES=(${FULL_MB})
#PREFETCHERS=(no readahead majority)
PREFETCHERS=(majority)

FULL_CLEAN=n

if [[ "${FULL_CLEAN}" = "y" ]]; then
    ./clean.sh
fi

./clean-app.sh
./build.sh metis

export WRMEM_CPUS=(1 2 4 8 16 24 28 32 40 48 56)

./remote.sh down
./remote.sh clean
./remote.sh build

TRIES=(1 2 3)

mkdir -p $OUT_PATH
for TRY in ${TRIES[@]}; do
    for M in ${MEMORIES[@]}; do
        for P in ${PREFETCHERS[@]}; do
            for C in ${WRMEM_CPUS[@]}; do
                MYPID=$( ps faux | grep 'qemu-system-x86' | grep -vw grep | awk '{ print $2 }' )
                echo $MYPID
                if [[ -n $MYPID ]]; then
                    kill -9 $MYPID
                fi

                ./remote.sh down
                sleep 2
                ./remote.sh up
                sleep 2

                FILE_OUT="${OUT_PATH}/wrmem-$M-$P-$C-$TRY.txt"
                echo ${FILE_OUT}
                install_timeout 3000 qemu-system-x86_64
                sleep 1
                MEMORY=$(expr $M + $MEM_EXTRA_MB)M PREFETCHER=$P ./run.sh /wrmem -s 5000 -p ${C} | tee ${FILE_OUT} &
                sleep 2
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
