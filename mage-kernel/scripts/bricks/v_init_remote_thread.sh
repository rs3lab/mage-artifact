#!/bin/bash

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/mind_linux/test_programs/multithreading

#$1: number of remote threads

echo "Initialize remote thread containers"
nohup unbuffer ./run_container.sh $1 > ./multith.log 2>&1 &
exit
