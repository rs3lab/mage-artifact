#!/bin/bash

if [[ -z $MIND_ROOT ]]; then
	echo '$MIND_ROOT not set!' >/dev/stderr
	exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/frontend

function last_digits () {
    echo $1 | awk -F '.' '{print $4}'
}

echo "Building MIND frontend (no output => success)"
chronic -e cmake .
chronic -e make

echo "Killing running mind frontend..."
killall -9 tna_disagg_switch_base >/dev/null 2>&1

echo "Starting frontend (tna_disagg_switch_base):"
date > frontend.log

# Remember, the frontend has no way of communicating with the MN. But that's
# OK; we set up a _dummy_ mn, so the frontend should ignore the $mn_data_ip arg.
nohup unbuffer taskset -c 0 build/tna_disagg_switch_base \
    "$(last_digits $cn_control_ip)" "$(last_digits $mn_control_ip)" \
    >> frontend.log 2>&1 &

echo "Waiting for 5 seconds..."
sleep 5
exit
