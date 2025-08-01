#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/sequential-write

cnthreads=$1
fhthreads=$2
bs=$3

log_dir="cn${cnthreads}-bs${bs}-logs/"

rm -rf logs
mkdir -p $log_dir
./test-one.zsh $fhthreads
mv logs/* $log_dir
rm -d logs
