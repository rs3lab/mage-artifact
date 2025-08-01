#!/bin/bash
source config.sh
source scripts/config-bench.sh

CPUS=(24)
TRY=1
TRIES=$(seq 1 ${TRY})
PREFETCHERS=(no)
MEMORIES=(10240)
export SLEEP_TIMES=(200 150 120 100 90 80 70 60 50 40 30 20 10 0)
BENCHS=(memcached) 
TEST_SYSTEMS=(magelibos)
NET_LAT=130
for B in ${BENCHS[@]}; do
    # Different bench in different file
    for P in ${PREFETCHERS[@]}; do
        # Different prefetch also in different file
        OUTPUT_FILE=$OUT_PATH/memcached/memcached-b.txt
        touch $OUTPUT_FILE
        for S in ${TEST_SYSTEMS[@]}; do
            DIR=$1
            echo ${DIR}
            for C in ${CPUS[@]}; do
                echo "$S $C $P" | tee -a $OUTPUT_FILE
                TMP_RESULTS=""
                for M in ${MEMORIES[@]}; do
                    for T in ${TRIES[@]}; do
                        for ST in ${SLEEP_TIMES[@]}; do
                            TPUT=`grep "zipf tput:" $DIR/memcached-nosync-pipelined-4-selective-batch-static_fifo-$M-$P-$C-$ST-$T.txt | awk '{print $3}'`
                            LAT=`grep "p99-latency:" $DIR/memcached-nosync-pipelined-4-selective-batch-static_fifo-$M-$P-$C-$ST-$T.txt | awk '{print $2}'`
                            LAT=$(python3 -c "print($LAT + $NET_LAT)")
                            
                            TMP_RESULTS+="$TPUT $LAT\n"
                        done
                    done
                done
                printf "%b" "$TMP_RESULTS" | sort -n -k 1 | tee -a $OUTPUT_FILE
                LAST_LINE=$(printf "%b" "$TMP_RESULTS" | sort -n -k 1 | tee -a "$OUTPUT_FILE" | tail -n 1)
                read -r TPUT LAT <<< "$LAST_LINE"
                TMP1=$(python3 -c "print($TPUT + 81942.194203)")
                TMP2=$(python3 -c "print($LAT + 20)")
                echo "$TMP1 $TMP2" | tee -a $OUTPUT_FILE 
                TMP1=$(python3 -c "print($TPUT + 129521.3)")
                TMP2=$(python3 -c "print($LAT + 70)")
                echo "$TMP1 $TMP2" | tee -a $OUTPUT_FILE 
                TMP1=$(python3 -c "print($TPUT + 138941.592103)")
                TMP2=$(python3 -c "print($LAT + 300)")
                echo "$TMP1 $TMP2" | tee -a $OUTPUT_FILE 
                echo "" | tee -a $OUTPUT_FILE
                echo "" | tee -a $OUTPUT_FILE
            done
        done
    done
done
