#!/usr/bin/env zsh

if [[ $# -ne 4 ]]; then 
	echo "./test-one.zsh cn fh cnqp fhqp"
	exit 1
fi
echo "./test-one.zsh cn=$1 fh=$2 cnqp=$3 fhqp=$4"

n_cnthreads=$1
n_fhthreads=$2
n_cnqp=$3
n_fhqp=$4

if (( n_cnthreads > n_cnqp )); then
	echo "error: not enough cnqps to support these many threads"
	exit 1
fi
if (( n_fhthreads > n_fhqp )); then
	echo "error: not enough fhqps to support these many threads"
	exit 1
fi

cd /home/sslee/mind_internal/mind_linux/roce_modules

local test_code="cn$n_cnthreads-fh$n_fhthreads-cnqp$n_cnqp-fhqp$n_fhqp"

mn='fbss4'
cn_nic='ib0'
mn_data_ip='10.10.10.202'
module_dir='/home/sslee/mind_internal/mind_linux/util_modules/'
log_root="./logs"
pre_log="$log_root/pre.${test_code}.log"
post_log="$log_root/post.${test_code}.log"
pre_hwcounter_log="$log_root/pre.hwcounter.${test_code}.log"
post_hwcounter_log="$log_root/post.hwcounter.${test_code}.log"

function safe_rmmod () {
	for module in "$@"; do
		sudo rmmod $module &>/dev/null || true
	done
}

function pgrep-mn () {
        ssh $mn 'pgrep -u root clean_rdma_serv'
}

function kill-mn () { 
        ssh $mn "~/mind_next/kill-mem-server"
}

function reset-mn () {
        echo 'test-one: restarting memory server...'
        kill-mn
        sleep 3
        ssh $mn "setup-network && ~/mind_next/run-mem-server"
        sleep 1
        printf 'test-one: restart complete. mn server pid is: %d\n' "$(pgrep-mn)"
}

function rrm () { 
	safe_rmmod roce4disagg
}

function rreset () {
        rrm
        reset-mn
        echo 'test-one: making roce (no output => success)'
        chronic make
}


echo "test-one: making pprint and pclean modules. no output = success"
cd $module_dir
chronic make clean
chronic make
safe_rmmod fbs_pprint fbs_pclean fbs_psample
cd - >/dev/null

mkdir -p $log_root

setup-network
rreset

sleep 10

echo "test-one: generating pre-test logfiles"
sudo insmod $module_dir/fbs_pclean.ko
safe_rmmod fbs_pclean
sudo dmesg --color=always &> $pre_log
sudo ethtool -S $cn_nic &>$pre_hwcounter_log

echo 'test-one: activating pp sampling'
sudo insmod $module_dir/fbs_psample.ko sample=1
safe_rmmod fbs_psample

# BENCHMARK TIME
echo 'test-one: inserting roce module'
sudo insmod roce4disagg.ko ip_addr=$mn_data_ip \
	num_cnthreads=$n_cnthreads num_fhthreads=$n_fhthreads \
        num_cnqps=$n_cnqp num_fhqps=$n_fhqp
echo 'test-one: waiting for benchmark completion.'
sleep 40s
rrm 

echo 'test-one: benchmark done, deactivating psample'
sudo insmod $module_dir/fbs_psample.ko sample=0
safe_rmmod fbs_psample

echo "test-one: generating post-test logfiles"
sudo ethtool -S $cn_nic &>$post_hwcounter_log

dmesg --color=always -w &> $post_log &
dmesg_pid=$!
sudo insmod $module_dir/fbs_pprint.ko
safe_rmmod fbs_pprint
sleep 3s
kill $dmesg_pid
wait $dmesg_pid

echo 'test-one: shutting down memory server'
sleep 3
kill-mn

sync $pre_log $post_log $pre_hwcounter_log $post_hwcounter_log
