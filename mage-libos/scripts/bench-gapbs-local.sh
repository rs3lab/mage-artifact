#!/bin/bash
source config.sh
source scripts/config-bench.sh

echo $MEM_EXTRA_MB

OUT_PATH="$OUT_PATH/gapbs/${DATE}"
ALGO=(pr)
GRAPH_TRIAL=1

declare -A ALGO_PARAMS
ALGO_PARAMS[pr]=" -f /mnt/twitter.sg -i1000 -t1e-4 "
ALGO_PARAMS[bc]=" -f /mnt/twitter.sg -i1 "

FULL_CLEAN=n

if [[ "${FULL_CLEAN}" = "y" ]]; then
    ./clean.sh
fi

./clean-app.sh
./build.sh gapbs

OMP_CPUS=(1 2 4 8 16 24 28 32 40 48 56)

mkdir -p $OUT_PATH
for A in ${ALGO[@]}; do
    for TRY in ${TRIES[@]}; do
        for C in ${OMP_CPUS[@]}; do
            FILE_OUT="${OUT_PATH}/gapbs-local-$A-$C-$TRY.txt"
            echo ${FILE_OUT}
            GOMP_CPU_AFFINITY=0-${C} OMP_NUM_THREADS=${C} ./apps/gapbs/gapbs/pr -f /mnt/twitter/twitter.sg -i1000 -t1e-4 -n3 | tee  ${FILE_OUT}
        done
    done
done
