#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/sequential-read

echo "Execute this on the VM host only!"

# HELPER FUNCTIONS

function set-params () {
	local cn=$1
	local bs=$2
	local lmem_mib=$3
	ssh $cn_control_sshname set-params 'test_mltthrd' $cn $bs $lmem_mib

	manager cn allonly
	sleep 1s
}

function run-test () {
  local cnthreads=$1
  local fhthreads=$2
  local bs=$3
  local lmem_mib=$4

  # reset the cluster (updating the kernel as well)
  reset-fbs
  sleep 5s

  # run test-one; time to start the application!
  # TODO(yash): shouldn't this use MIND_ROOT?
  ssh $cn_control_sshname zsh \
	'/home/sslee/rfbs/apps/sequential-read/test-one.zsh' \
	$cnthreads $fhthreads $bs $lmem_mib
}


# BEGIN TESTS: GENERATE FIGURE 14

# rebuild application
local cn=4            # 4 eviction threads
local bs=256          # 256 batch size
local lmem_mib='1024' # 1 GiB DRAM Cache Size TODO(yash): confirm this

set-params $cn $bs $lmem_mib

# Run the tests!

for fh in 1 2 4 8 16 32 40 48; do
	if (( fh + cn > 52 )); then
		continue
	fi

	run-test $cn $fh $bs $lmem_mib
	# the "1" suffix indicates "run 1". Needed for our output scripts. 
	fetch-test-logs "cn$cn-fh$fh-bs$bs-lmem_mib$lmem_mib-logs.1"
done
