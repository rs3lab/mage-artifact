#!/bin/bash
source config.sh
source scripts/config-bench.sh

CPUS=(48)
TRY=1
TRIES=$(seq 1 ${TRY})
PREFETCHERS=(no)
#MEMORIES=(1300 2600 3900 5200 6500 7800 9100 10400 11700 13000)
MEMORIES=(1500 3000 4500 6000 7500 9000 10500 12000 13500 15000 18000)
MAX_MEMORY=${MEMORIES[-1]}
BENCHS=(xsbench) 
#TEST_SYSTEMS=(mage dilos hermit)
TEST_SYSTEMS=(magelibos)
for B in ${BENCHS[@]}; do
    # Different bench in different file
    for P in ${PREFETCHERS[@]}; do
        # Different prefetch also in different file
        OUTPUT_FILE=$OUT_PATH/xsbench/xsbench.txt
        touch $OUTPUT_FILE
        for S in ${TEST_SYSTEMS[@]}; do
            DIR=$1
            echo ${DIR}
            for C in ${CPUS[@]}; do
                echo "$S $C $P" | tee -a $OUTPUT_FILE
                for M in ${MEMORIES[@]}; do
                    RESULT=0
                    for T in ${TRIES[@]}; do
                        #TMP=`grep Runtime $DIR/xsbench-$M-$P-$C.txt | awk '{print $2}'`
                        TMP=`grep Runtime $DIR/xsbench-nosync-pipelined-4-selective-patr-static_fifo-$M-$P-$C-1.txt | awk '{print $2}'`
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
            done
        done
    done
done
