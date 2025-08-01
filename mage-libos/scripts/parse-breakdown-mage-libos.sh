#!/bin/bash
source config.sh
source scripts/config-bench.sh

CPUS=(48)
TRY=1
TRIES=$(seq 1 ${TRY})
PREFETCHERS=(no)
MEMORIES=(2200 4400 6600 8800 11000 13200 15400 17600 19800 22000) # Kron
MAX_MEMORY=${MEMORIES[-1]}
BENCHS=(pr) 
OUTPUT_FILE=$OUT_PATH/gapbs/gapbs-breakdown.txt
DIR=$1
BATCH=256
echo ${DIR}
function parse_async(){
    local A=$1
    local TRY=$2
    local P=$3
    local L=$4
    local T=$5
    local C=$6
    local GL=$7
    local SUFFIX=$8
    for M in ${MEMORIES[@]}; do
        RESULT=0
        for T in ${TRIES[@]}; do
            TMP=`grep Average $DIR/gapbs-kron-nosync-pipelined-4-selective-${BATCH}-$T-$L-$A-$M-$P-$C-$T-${SUFFIX}.txt | awk '{print $3}'`
            echo $DIR 
            echo $TMP
            if [[ -z $TMP ]]; then
                TMP=0
            fi
            RESULT=$(python3 -c "print($RESULT + $TMP)")
        done
        if [[ "$RESULT" != "0" ]]; then
            RESULT=$(python3 -c "print(round(3600*$TRY/$RESULT , 2))")
        fi
        PERCENT=$(python3 -c "print(int(100 * $M/$MAX_MEMORY))")
        echo "$PERCENT $RESULT" | tee -a $OUTPUT_FILE
    done
    echo "" | tee -a $OUTPUT_FILE
    echo "" | tee -a $OUTPUT_FILE
}
function parse_sync(){
    local A=$1
    local TRY=$2
    local P=$3
    local L=$4
    local T=$5
    local C=$6
    local GL=$7
    local SUFFIX=$8
    for M in ${MEMORIES[@]}; do
        RESULT=0
        for T in ${TRIES[@]}; do
            TMP=`grep Average $DIR/gapbs-kron-sync-pipelined-1-$T-$L-$A-$M-$P-$C-$T-${SUFFIX}.txt | awk '{print $3}'`
            echo $DIR 
            echo $TMP
            if [[ -z $TMP ]]; then
                TMP=0
            fi
            RESULT=$(python3 -c "print($RESULT + $TMP)")
        done
        if [[ "$RESULT" != "0" ]]; then
            RESULT=$(python3 -c "print(round(3600*$TRY/$RESULT , 2))")
        fi
        PERCENT=$(python3 -c "print(int(100 * $M/$MAX_MEMORY))")
        echo "$PERCENT $RESULT" | tee -a $OUTPUT_FILE
    done
    echo "" | tee -a $OUTPUT_FILE
    echo "" | tee -a $OUTPUT_FILE
}
echo "l2" | tee -a $OUTPUT_FILE
parse_async pr 1 no static_fifo batch 48 no all
echo "lru" | tee -a $OUTPUT_FILE
parse_async pr 1 no static_fifo batch 48 yes nol2
echo "pipelined" | tee -a $OUTPUT_FILE
parse_async pr 1 no static_lru batch 48 yes nolru
echo "baseline" | tee -a $OUTPUT_FILE
parse_sync pr 1 no static_lru batch 48 yes nopipelined