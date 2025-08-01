#!/bin/bash
source config.sh
source scripts/config-bench.sh

set -euo

FULL_CLEAN=n

MEMCACHED_TOTAL_SIZE=20000 #20G
MEMCACHED_THREADS=(1 2 4 8 12)
MEMORIES=(20000 15000 10000 5000)

OUT_PATH="$OUT_PATH/memcached/${DATE}"

declare -A OPERATION
OPERATION[prepare]="-s ${IP} -p 11211 -n 10000 -P memcache_text -t 8 --ratio 100:1 --key-pattern=P:P --data-size=10240"
OPERATION[set]="-s ${IP} -p 11211 -n 100000 -P memcache_text -t 8 --ratio 100:1 --key-pattern=R:R --data-size=1024 >> ${OUT_PATH}/set_log"
OPERATION[get]="-s ${IP} -p 11211 -n 100000 -P memcache_text -t 8 --ratio 1:100 --key-pattern=R:R --data-size=1024 >> ${OUT_PATH}/get_log"

echo $MEM_EXTRA_MB

# Mkdir on the remote side
./remote.sh custom "mkdir -p ${OUT_PATH}"

echo "Create dir on remote side"
for T in ${MEMCACHED_THREADS[@]}; do
    for M in ${MEMORIES[@]}; do
        echo $M
        echo $P
        MYPID=$( ps faux | grep 'memcached' | grep -vw grep | awk '{ print $2 }' )
        echo $MYPID
        if [[ -n $MYPID ]]; then
            kill -9 $MYPID
        fi
        echo $(($M*1024*1024)) > /sys/fs/cgroup/memory/bench/memory.limit_in_bytes
        cgexec --sticky -g memory:bench ./dilos/apps/memcached/memcached -u root -t $T -m${MEMCACHED_TOTAL_SIZE} &

        sleep 20
        echo "START MEMCACHED"
        ./remote.sh custom "memtier_benchmark ${OPERATION[prepare]}"
        sleep 2
        echo "Prepare done"
        ./remote.sh custom "memtier_benchmark ${OPERATION[get]}"
        sleep 2
        echo "Test done"
    done
done
MYPID=$( ps faux | grep 'memcached' | grep -vw grep | awk '{ print $2 }' )
echo $MYPID
if [[ -n $MYPID ]]; then
    kill -9 $MYPID
fi
