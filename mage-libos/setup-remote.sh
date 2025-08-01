#!/bin/bash
source config.sh

echo "Setup Frequency"
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
echo ${FREQ}| sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq
echo ${FREQ}| sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq

echo "Setup IP"
ip addr add ${MS_IP}/${PREFIX} dev ${MS_ETH_IF}
ip link set ${MS_ETH_IF} up

echo "Setup HugeTLB"
hugeadm --pool-pages-min 2M:200G
hugeadm --create-mounts

echo "Setup RDMA"
ip addr add ${MS_RDMA_IP}/${PREFIX} dev ${MS_RDMA_IF}
ip link set ${MS_RDMA_IF} up
