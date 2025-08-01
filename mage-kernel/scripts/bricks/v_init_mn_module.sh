#!/bin/zsh

set -u

if [[ -z $MIND_ROOT ]]; then
	echo '$MIND_ROOT not set!' >/dev/stderr
	exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/scripts/bricks

echo "Setting up our network interfaces (IP/ARP)"
./v_init_network.sh

echo "Restarting FBS server..."
$MIND_ROOT/scripts/mn/kill-mem-server
sleep 5
$MIND_ROOT/scripts/mn/run-mem-server
