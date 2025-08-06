#!/bin/zsh

# same for both nodes :)

if [[ -z $MIND_ROOT ]]; then
	echo '$MIND_ROOT not set!' >/dev/stderr
	exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/scripts/bricks

if [[ "$(hostname)" = $mn_vm_hostname ]]; then
        nic=$mn_nic
        laddr=$mn_data_ip
        rmac=$cn_mac
        raddr=$cn_data_ip
elif [[ "$(hostname)" = $cn_vm_hostname ]]; then
        nic=$cn_nic
        laddr=$cn_data_ip
        rmac=$mn_mac
        raddr=$mn_data_ip
else
        echo 'Unknown hostname, not sure what IP to assign!' >/dev/stderr
        exit 1
fi

sudo ip link set $nic up 

if [[ "$using_roce" = 'true' ]]; then
	# NIC MTU should be larger than (page size + frame headers).
	# RoCE only supports MTUs in powers of two. It'll choose the largest MTU less
	# than the iface MTU.
	sudo ip link set $nic mtu 4200

	# Confirm that RoCE MTU moved
	roce_mtu="$(ibv_devinfo -d mlx5_0 | grep 'active_mtu' | awk '{print $2}')"
	if [[ $roce_mtu -ne 4096 ]]; then
		echo "$0: error: low infiniband mtu set! ($roce_mtu)"
		echo "$0: your performance will be lower than expected"
		exit 1
	fi

	# ARP entry for other node. Not needed for IB. 
	sudo ip neigh replace $raddr dev $nic lladdr $rmac nud permanent
else 
	sudo systemctl start opensm
	sleep 3
fi

sudo ip addr replace "$laddr/$data_subnet_prefix" dev $nic
