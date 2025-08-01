#!/usr/bin/env zsh

if [[ $# -ne 1 ]]; then 
	echo $0 '<nthreads>'
	exit 1
fi

echo "./test-one.zsh $1"

function safe_rmmod () {
	for module in "$@"; do
		sudo rmmod $module &>/dev/null || true
	done
}

n_threads=$1

# in MiB
array_size=4096
warmup_iterations=30000000
test_iterations=10000000
test_skew=0.5

cn_nic='ib0'
module_dir='/home/sslee/mind_internal/mind_linux/util_modules/'
log_root='./logs'
memcached_log="$log_root/memcached.$n_threads.log"
output_log="$log_root/$n_threads.log"
pre_log="$log_root/pre.$n_threads.log"
post_log="$log_root/post.$n_threads.log"
pre_hwcounter_log="$log_root/pre.hwcounter.$n_threads.log"
post_hwcounter_log="$log_root/post.hwcounter.$n_threads.log"

(cd mclient; chronic make)

mkdir -p $log_root

echo "making fbs_pprint and fbs_pclean modules. no output = success"
cd $module_dir
chronic make clean
chronic make
safe_rmmod fbs_pprint fbs_pclean fbs_psample
cd - >/dev/null

echo "test-one: cleaning up profile points"
sudo insmod $module_dir/fbs_psample.ko sample=0
safe_rmmod fbs_psample
sudo insmod $module_dir/fbs_pclean.ko
safe_rmmod fbs_pclean
sudo dmesg --color=always &> $pre_log

echo "test-one: spawning memcached with $n_threads threads and $array_size MiB."
memcached --threads $n_threads --memory-limit $array_size -p 11211 &> $memcached_log & 
memcached_pid=$!

sleep 5

echo 'test-one: warming up memcached cache'
numactl -N 1 -m 1 ./mclient/mclient 127.0.0.1 11211 0 $warmup_iterations

echo 'test-one: spawning memclient for benchmark'
numactl -N 1 -m 1 unbuffer \
	./mclient/mclient 127.0.0.1 11211 $test_skew $test_iterations &> $output_log &
mclient_pid=$!

echo 'test-one: waiting for benchmark to begin'
while true; do
	if grep -q 'BEGIN_BENCHMARK' $output_log; then
		break
	fi
	sleep 0.25
done

echo "test-one: benchmark ready, activating psample"
date +%s > $pre_hwcounter_log
sudo ethtool -S $cn_nic &>>$pre_hwcounter_log
sudo insmod $module_dir/fbs_psample.ko sample=1
safe_rmmod fbs_psample

echo 'test-one: waiting for benchmark to end'
while true; do
	if grep -q 'END_BENCHMARK' $output_log; then
		break
	fi
	sleep 0.25
done

echo 'test-one: benchmark done, deactivating psample'
sudo insmod $module_dir/fbs_psample.ko sample=0
safe_rmmod fbs_psample

echo "test-one: generating post-test logfiles"
date +%s > $post_hwcounter_log
sudo ethtool -S $cn_nic &>>$post_hwcounter_log

dmesg --color=always -w &> $post_log &
dmesg_pid=$!
sudo insmod $module_dir/fbs_pprint.ko
safe_rmmod fbs_pprint
sleep 3s
kill $dmesg_pid
wait $dmesg_pid

wait $mclient_pid
kill $memcached_pid
wait $memcached_pid

echo "test-one: done."
