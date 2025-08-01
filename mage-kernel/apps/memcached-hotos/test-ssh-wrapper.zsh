#!/bin/zsh -e

cnthreads=$1
fhthreads=$2
bs=$3

log_dir="cn${cnthreads}-bs${bs}-logs/"

cd ~/mind_internal/mind_linux/test_programs/01g_yash_memcached

rm -rf logs
mkdir -p $log_dir
./test-one.zsh $fhthreads
mv logs/* $log_dir
rm -d logs
