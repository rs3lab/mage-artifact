#!/bin/bash
source config.sh
source scripts/config-bench.sh

CPUS=(48)
TRY=1
TRIES=$(seq 1 ${TRY})
PREFETCHERS=(no readahead)
MEMORIES=(23500 21150 18800 16450 14100 11750 9400 7050 4700 2350)
MAX_MEMORY=${MEMORIES[0]}
TEST_SYSTEM=magelibos
STRIDE=1

OUTPUT_FILE=$OUT_PATH/seq_scan/seq_scan.txt
touch $OUTPUT_FILE
DIR=$1
echo ${DIR}
for C in ${CPUS[@]}; do
    for P in ${PREFETCHERS[@]}; do
        echo "$TEST_SYSTEM $C $P" | tee -a $OUTPUT_FILE
            for M in ${MEMORIES[@]}; do
            RESULT=0
            for T in ${TRIES[@]}; do
                #seq-scan-selective-4-pipelined-nosync-patr-static-fifo-21150-no-48-1-1.txt
                TMP=`grep tput $DIR/seq-scan-selective-4-pipelined-nosync-patr-static-fifo-$M-$P-$C-$T-$STRIDE.txt | head -n 1| awk '{print $2}'`
                echo $DIR 
                echo $TMP
                if [[ -z $TMP ]]; then
                    TMP=0
                fi
                RESULT=$(python3 -c "print($RESULT + $TMP)")
            done
            if [[ "$RESULT" != "0" ]]; then
                RESULT=$(python3 -c "print(round($RESULT / $TRY, 2))")
            fi
            PERCENT=$(python3 -c "print(int(100 * $M/$MAX_MEMORY))")
            echo "$PERCENT $RESULT" | tee -a $OUTPUT_FILE
        done
        echo "" | tee -a $OUTPUT_FILE
        echo "" | tee -a $OUTPUT_FILE
    done
done
