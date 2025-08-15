#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/memcached

echo "Execute this on the VM host only!"

function set-params () {
	local lmem_mib=$1
	local sleep_time=$2

	# rebuild application w/ new params
	ssh $cn_control_sshname "SLEEP_TIME=$sleep_time make -C \$MIND_ROOT/apps/memcached/"

	# rebuild kernel w/ new params
	ssh $cn_control_sshname set-params 'memcached' $cn $bs $lmem_mib 0.8
	manager cn allonly
	sleep 1s

	# reset the cluster (updating the kernel as well)
	reset-fbs
	sleep 5s
}

# A single test "run".
function run-test () {
  local cnthreads=$1
  local fhthreads=$2
  local bs=$3
  local lmem_mib=$4
  local sleep_time=$5

  # run test-one; time to start the application!
  ssh $cn_control_sshname "zsh \$MIND_ROOT/apps/memcached/test-one.zsh $cnthreads $fhthreads $bs $lmem_mib $sleep_time"
}

cn=4
fh=12
bs=256

lmem_mibs=(20480 18432 16384 14336 12288 10240 8192 8000)
lmem_mibs=(20480 18432 16384 14336 12288 10240 8000 7000 6000)
sleep_times=(30 25 15 12 10 5 0 0 0)

# zip("lmem_mibs", "sleep_times")
for (( i = 1; i <= $#lmem_mibs; i++ )); do 
	local lmem_mib=$lmem_mibs[$i]
	local sleep_time=$sleep_times[$i]

	if (( fh + cn > 52 )); then
		continue
	fi

	set-params $lmem_mib $sleep_time

	run-test $cn $fh $bs $lmem_mib $sleep_time
	fetch-test-logs "cn$cn-fh$fh-bs$bs-lmem_mib$lmem_mib-sleep$sleep_time-logs.1"
done
