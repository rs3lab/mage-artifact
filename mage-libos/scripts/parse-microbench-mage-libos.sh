#!/bin/bash
source config.sh
source scripts/config-bench.sh

CPUS=(1 2 4 8 16 24 28 32 40 48)
TRY=1
TRIES=$(seq 1 ${TRY})
PREFETCHERS=(no)
MEMORIES=(7168)
TEST_SYSTEMS=(magelibos)
STRIDE=1

OUTPUT_FILE_LAT=$OUT_PATH/microbench/microbench-lat.txt
OUTPUT_FILE_TPUT=$OUT_PATH/microbench/microbench-tput.txt
touch $OUTPUT_FILE_LAT
touch $OUTPUT_FILE_TPUT
for S in ${TEST_SYSTEMS[@]}; do
    DIR=$1
    echo ${DIR}
    for M in ${MEMORIES[@]}; do
        for P in ${PREFETCHERS[@]}; do
            echo "$S $M $P" | tee -a $OUTPUT_FILE_LAT
            echo "$S $M $P" | tee -a $OUTPUT_FILE_TPUT
            for C in ${CPUS[@]}; do
                RESULT_LAT=0
                RESULT_TPUT=0
                for T in ${TRIES[@]}; do
                    TMP_LAT=`grep "^tput:" $DIR/microbench-selective-4-pipelined-nosync-patr-static-fifo-$M-$P-$C-$T-$STRIDE.txt | awk '{print $2}'`
                    TMP_TPUT=`grep "p99-latency:" $DIR/microbench-selective-4-pipelined-nosync-patr-static-fifo-$M-$P-$C-$T-$STRIDE.txt | awk '{print $2}'`
                    echo $DIR 
                    echo $TMP_LAT
                    echo $TMP_TPUT
                    if [[ -z $TMP_LAT ]]; then
                        TMP_LAT=0
                    fi
                    if [[ -z $TMP_TPUT ]]; then
                        TMP_TPUT=0
                    fi
                    RESULT_LAT=$(python3 -c "print($RESULT_LAT + $TMP_LAT)")
                    RESULT_TPUT=$(python3 -c "print($RESULT_TPUT + $TMP_TPUT)")
                done
                if [[ "$RESULT_LAT" != "0" ]]; then
                    RESULT_LAT=$(python3 -c "print(round($RESULT_LAT / $TRY, 2))")
                fi
                if [[ "$RESULT_TPUT" != "0" ]]; then
                    RESULT_TPUT=$(python3 -c "print(round($RESULT_TPUT / $TRY, 2))")
                fi
                echo "$C $RESULT_LAT" | tee -a $OUTPUT_FILE_LAT
                echo "$C $RESULT_TPUT" | tee -a $OUTPUT_FILE_TPUT
            done
            echo "" | tee -a $OUTPUT_FILE_LAT
            echo "" | tee -a $OUTPUT_FILE_LAT
            echo "" | tee -a $OUTPUT_FILE_TPUT
            echo "" | tee -a $OUTPUT_FILE_TPUT
        done
    done
done
