#!/bin/bash
source config.sh

echo "Setup Frequency"
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
echo ${FREQ}| sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq
echo ${FREQ}| sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq

echo "Setup Bridge"
ip addr add ${IP}/${PREFIX} dev ${ETH_IF}
brctl addbr dilosbr
brctl addif dilosbr ${ETH_IF}
brctl show
ip addr add ${GW}/${PREFIX} dev dilosbr
ip link set ${ETH_IF} up
ip link set dilosbr up
sysctl -w net.ipv4.ip_forward=1
echo 0 | sudo tee  /proc/sys/net/bridge/bridge-nf-call-iptables

echo "Setup RDMA"
ip addr add ${RDMA_IP}/${PREFIX} dev ${RDMA_IF}
ip link set ${RDMA_IF} up
