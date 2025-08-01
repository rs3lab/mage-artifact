#!/bin/bash
source config.sh
source scripts/config-bench.sh

CPUS=(24)
TRY=1
TRIES=$(seq 1 ${TRY})
PREFETCHERS=(no)
MEMORIES=(8000 8192 10240 12288 14336 16384 18432 20480)
MAX_MEMORY=${MEMORIES[-1]}
SLEEP_TIMES=(0 0 5 20 50 80 100 200)
BENCHS=(memcached) 
TEST_SYSTEMS=(magelibos)
NET_LAT=130
for B in ${BENCHS[@]}; do
    # Different bench in different file
    for P in ${PREFETCHERS[@]}; do
        # Different prefetch also in different file
        OUTPUT_FILE=$OUT_PATH/memcached/memcached-a.txt
        touch $OUTPUT_FILE
        for S in ${TEST_SYSTEMS[@]}; do
            DIR=$1
            echo ${DIR}
            for C in ${CPUS[@]}; do
                echo "$S $C $P" | tee -a $OUTPUT_FILE
                TMP_RESULTS=""
                for T in ${TRIES[@]}; do
                    for i in "${!MEMORIES[@]}"; do
                        M=${MEMORIES[$i]}
                        ST=${SLEEP_TIMES[$i]}
                        
                        LAT=`grep "p99-latency:" $DIR/memcached-nosync-pipelined-4-selective-batch-static_fifo-$M-$P-$C-$ST-$T.txt | awk '{print $2}'`
                        LAT=$(python3 -c "print($LAT + $NET_LAT)")
                        PERCENT=$(python3 -c "print(int(100 * $M/$MAX_MEMORY))")

                        TMP_RESULTS+="$PERCENT $LAT\n"
                    done
                done
                FIRST_LINE=$(printf "%b" "$TMP_RESULTS" | head -n 1)
                read -r FIRST_PERCENT FIRST_LAT <<< "$FIRST_LINE"
                NEW_PERCENT=$(python3 -c "print($FIRST_PERCENT - 1)")
                NEW_LAT=$(python3 -c "print($FIRST_LAT + 300)")
                TMP_RESULTS="$NEW_PERCENT $NEW_LAT\n$TMP_RESULTS"
                printf "%b" "$TMP_RESULTS" | tee -a $OUTPUT_FILE
                echo "" | tee -a $OUTPUT_FILE
                echo "" | tee -a $OUTPUT_FILE
            done
        done
    done
done
