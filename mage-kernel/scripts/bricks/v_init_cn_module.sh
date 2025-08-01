#!/bin/zsh

set -u

if [[ -z $MIND_ROOT ]]; then
	echo '$MIND_ROOT not set!' >/dev/stderr
	exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/scripts/bricks

echo "Setting up our network interfaces"
$MIND_ROOT/scripts/bricks/v_init_network.sh

# echo "Configuring network stack parameters:"

# TODO: These stopped working once we switched from MLNX OFED drivers to
# upstream drivers. Figure out why.
#
# Sets quality-of-service parameters for the MLNX NIC. Eg: flow control, etc.
# sudo mlnx_qos -i $2 --trust dscp >/dev/null
# sudo mlnx_qos -i $2 --pfc 0,0,0,1,0,0,0,0 >/dev/null

# Configure how long to spin when waiting for NIC packets.
#echo 70 | sudo tee /proc/sys/net/core/busy_read >/dev/null
#echo 70 | sudo tee /proc/sys/net/core/busy_poll >/dev/null
#echo 32768 | sudo tee /proc/sys/net/core/rps_sock_flow_entries >/dev/null

echo "Starting TCP Daemon"
cd $MIND_ROOT/mind_linux/test_programs/90_mind_daemons/05_tcp_daemon
sudo nohup ./run_tcp_daemon.sh $frontend_ip > tcp_daemon.output &
disown

cd $MIND_ROOT/mind_linux/roce_modules

echo "Making MIND RoCE Module..."
$MIND_ROOT/scripts/bricks/v_build_roce_module.sh

echo "Inserting roce4disagg.ko module (params ip_addr=$cn_data_ip frontend_ip_addr=$frontend_ip)"
chronic sudo insmod roce4disagg.ko "ip_addr=$cn_data_ip" "frontend_ip_addr=$frontend_ip"
