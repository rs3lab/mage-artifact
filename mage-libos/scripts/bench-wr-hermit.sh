#!/bin/bash
source config.sh
source scripts/config-bench.sh

set -euo

echo $MEM_EXTRA_MB

OUT_PATH="$OUT_PATH/wrmem/${DATE}"
MEMORIES=(30000 22500 15000 7500)
#MEMORIES=(${FULL_MB})
TRIES=(1 2 3)

FULL_CLEAN=n

export CPUS=(1 2 4 8 16 24 28 32 40 48 56)

mkdir -p $OUT_PATH
for TRY in ${TRIES[@]}; do
for M in ${MEMORIES[@]}; do
    for C in ${CPUS[@]}; do
	FILE_OUT="${OUT_PATH}/wrmem-$M-$C-$TRY.txt"
	echo ${FILE_OUT}
	sleep 1
	echo $(($M*1024*1024)) > /sys/fs/cgroup/memory/bench/memory.limit_in_bytes
	#Need to check whether it works
	cgexec --sticky -g memory:bench ./apps/metis/metis/obj/app/wrmem -s 5000 -p${C} | tee ${FILE_OUT}
    done
done
done
