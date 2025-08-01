#!/usr/bin/env zsh

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/sequential-cksum

echo "./test-one.zsh $1 $2"

continuous=0
if [[ "$1" = '--continuous' ]]; then
	continuous=1
	shift
fi

n_threads=$1
array_size='4294967296' # 4 GiB
log_root='./logs'

output_log="$log_root/$n_threads.log"
pre_log="$log_root/pre.$n_threads.log"
post_log="$log_root/post.$n_threads.log"
pre_hwcounter_log="$log_root/pre.hwcounter.$n_threads.log"
post_hwcounter_log="$log_root/post.hwcounter.$n_threads.log"

mkdir -p $log_root

echo "making program. no output = success."
chronic make clean
chronic make

echo "test-one: generating pre-test logfiles"
psample 0
pclean
sudo dmesg --color=always &> $pre_log

echo "test-one: spawning test_mltthrd with $n_threads and $array_size bytes."
nohup unbuffer ./bin/test_mltthrd $n_threads $array_size &> $output_log &
bench_pid=$!

echo 'test-one: waiting for array allocation.'
while true; do
	if grep -q 'ALLOC_DONE' $output_log; then
		break
	fi
	sleep 0.5
done

echo 'test-one: waiting for benchmark to begin'
while true; do
	if grep -q 'BEGIN_BENCHMARK' $output_log; then
		break
	fi
	sleep 0.5
done

echo "test-one: benchmark started, activating psample"
date +%s > $pre_hwcounter_log
sudo ethtool -S $cn_nic &>>$pre_hwcounter_log
psample 1

i=1
echo 'test-one: waiting for benchmark to end'
while true; do
	if grep -q 'END_BENCHMARK' $output_log; then
		break
	fi
	sleep 0.9

	if [[ "$continuous" -eq 1 ]]; then
		pprint
		sudo dmesg > "$log_root/during-$i.$n_threads.log"
		(( i++ ))
	fi
done

echo 'test-one: benchmark done, deactivating psample'
psample 0

echo "test-one: generating post-test logfiles"
date +%s > $post_hwcounter_log
sudo ethtool -S $cn_nic &>>$post_hwcounter_log

dmesg --color=always -w &> $post_log &
dmesg_pid=$!
pprint
sleep 3s
kill $dmesg_pid
wait $dmesg_pid

sync $pre_log $post_log $pre_hwcounter_log $post_hwcounter_log
echo "test-one: done."
