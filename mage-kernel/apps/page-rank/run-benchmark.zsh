#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/page-rank

echo "Execute this on the VM host only!"


# COMPILE THE BENCHMARK APPLICATION.
ssh $cn_control_sshname \
	'cd $MIND_ROOT/apps/page-rank/gapbs && make && if [[ -f pr ]]; then mv pr gapbs_pr; fi'

# RUN THE TESTS
echo "Running tests!"
./kron-memscan.test-ssh.zsh

# TODO: Parse the data
