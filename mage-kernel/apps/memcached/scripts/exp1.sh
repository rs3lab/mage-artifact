#!/bin/bash
source config.sh

DATE=$(date +"%m.%d:%H.%M")

OUT_PATH="${SOURCE_DIR}/bench_results/exp1/${DATE}"
TOTAL_MEM=20285996540 # 20 gb

SLEEP_TIMES=(200 150 120 100 90 80 70 60 50 40 30 20 10 0)
#SLEEP_TIMES=(0)

BENCH="./memcached"
stime=0

pushd $SOURCE_DIR
mkdir -p $OUT_PATH

for stime in ${SLEEP_TIMES[@]}; do
    
    make clean
    stime=$stime make         
    
    echo "Running for time: $stime"
    M=$(echo "$TOTAL_MEM * 50 / 100" | bc)
        
    # Limit the cgroup
    echo $M > /sys/fs/cgroup/memory/bench/memory.limit_in_bytes

    sleep 1
    taskset -c 0-24 cgexec --sticky -g memory:bench $BENCH -u root -t 12 -m200000 | tee "$OUT_PATH/$stime.txt"
done
popd
