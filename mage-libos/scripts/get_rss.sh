#!/usr/bin/bash
set -euo
INTERVAL=0.1
TIME=20
LOOP=`python3 -c "print('%d' % (${TIME} / ${INTERVAL}))"`
APP="gups-hotset-move"
echo $LOOP

for i in `seq 1 $LOOP`; do
    echo $i
    mem=`ps aux | grep ${APP} | grep -v grep | awk '{print $6}'`
    echo $mem >> rss
    sleep $INTERVAL
done
