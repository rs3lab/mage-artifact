#!/bin/bash
source config.sh
source scripts/config-bench.sh

set -euo

echo $MEM_EXTRA_MB

OUT_PATH="$OUT_PATH/minorfault/${DATE}"
#MEMORIES=(${FULL_MB})
MEMORIES=(20480)
TRIES=(1)
STRIDES=(1)

FULL_CLEAN=n

export CPUS=(1 2 4 8 16 24 28 32 40 48 56)

mkdir -p $OUT_PATH
for TRY in ${TRIES[@]}; do
for S in "${STRIDES[@]}"; do
for M in ${MEMORIES[@]}; do
    for C in ${CPUS[@]}; do
	FILE_OUT="${OUT_PATH}/microbench-$M-$C-$TRY-$S.txt"
	echo ${FILE_OUT}
	sleep 1
	#echo $(($M*1024*1024)) > /sys/fs/cgroup/memory/bench/memory.limit_in_bytes
	#Need to check whether it works
	/home/yupan/sys_test/page_fault -t $C -d -w -s $S -m  | tee ${FILE_OUT}
    done
done
done
done
