#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/memcached

echo "Execute this on the VM host only!"

function rebuild-kernel () { 
	local cn=$1
	local fh=$2
	local bs=$3
	local lmem_mib=$4

	# rebuild kernel w/ new params
	ssh $cn_control_sshname set-params 'memcached' $cn $bs $lmem_mib 0.8
	manager cn allonly
	sleep 1s
}

function rebuild-memcached () { 
	local sleep_time=$1

	# rebuild application w/ new params
	ssh $cn_control_sshname "make -C \$MIND_ROOT/apps/memcached/ clean"
	ssh $cn_control_sshname "SLEEP_TIME=$sleep_time make -C \$MIND_ROOT/apps/memcached/ memcached"
}

function set-params () {
	local cn=$1
	local fh=$2
	local bs=$3
	local lmem_mib=$4
	local sleep_time=$5

	rebuild-memcached $sleep_time
	rebuild-kernel $cn $fh $bs $lmem_mib

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
  timeout 10m \
  	ssh $cn_control_sshname "zsh \$MIND_ROOT/apps/memcached/test-one.zsh $cnthreads $fhthreads $bs $lmem_mib $sleep_time"
}

cn=4
fh=12
bs=256

# -----------------------------
# COLLECT THE DATA FOR FIGURE A
# -----------------------------

lmem_mibs=(20480 18432 16384 14336 12288 10240 8000 7000 6000)
sleep_times=(30 25 15 12 10 5 0 0 0)

# zip("lmem_mibs", "sleep_times")
for (( i = 1; i <= $#lmem_mibs; i++ )); do 
	local lmem_mib=$lmem_mibs[$i]
	local sleep_time=$sleep_times[$i]

	if (( fh + cn > 52 )); then
		continue
	fi

	set-params $cn $fh $bs $lmem_mib $lmem_mib $sleep_time
	run-test $cn $fh $bs $lmem_mib $sleep_time
	fetch-test-logs "cn$cn-fh$fh-bs$bs-lmem_mib$lmem_mib-sleep$sleep_time-logs.1"
done

# -----------------------------
# COLLECT THE DATA FOR FIGURE B
# -----------------------------

cn=4
fh=12
bs=256
lmem_mib=10240
sleep_times=(200 150 120 100 90 80 70 60 50 40 30 20 10 0)

# set new lmem_mib
rebuild-kernel $cn $fh $bs $lmem_mib
reset-fbs
sleep 5s

for sleep_time in $sleep_times; do 
	rebuild-memcached $sleep_time
	run-test $cn $fh $bs $lmem_mib $sleep_time
	fetch-test-logs "cn$cn-fh$fh-bs$bs-lmem_mib$lmem_mib-sleep$sleep_time-logs.2"
done
