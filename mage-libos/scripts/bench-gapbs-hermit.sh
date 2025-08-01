#!/bin/bash
source config.sh
source scripts/config-bench.sh

set -euo

echo $MEM_EXTRA_MB

OUT_PATH="$OUT_PATH/gapbs/${DATE}"
MEMORIES=(13000 9750 6500 3250)
#MEMORIES=(${FULL_MB})
ALGO=(pr)
GRAPH_TRIAL=3

declare -A ALGO_PARAMS
ALGO_PARAMS[pr]=" -f /mnt/twitter/twitter.sg -i1000 -t1e-4 "
ALGO_PARAMS[bc]=" -f /mnt/twitter/twitter.sg -i1 "

FULL_CLEAN=n

export OMP_CPUS=(1 2 4 8 16 24 28 32 40 48 56)

mkdir -p $OUT_PATH
for A in ${ALGO[@]}; do
    for TRY in ${TRIES[@]}; do
        for M in ${MEMORIES[@]}; do
            for C in ${OMP_CPUS[@]}; do
                FILE_OUT="${OUT_PATH}/gapbs-$A-$M-$C-$TRY.txt"
                echo ${FILE_OUT}
                sleep 1
                echo $(($M*1024*1024)) > /sys/fs/cgroup/memory/bench/memory.limit_in_bytes
                D=$((C-1))
                #Need to check whether it works
                GOMP_CPU_AFFINITY=0-$D OMP_NUM_THREADS=$C cgexec --sticky -g memory:bench ./apps/gapbs/gapbs/${A} ${ALGO_PARAMS[$A]} -n${GRAPH_TRIAL} | tee ${FILE_OUT}
            done
        done
    done
done
