# This script acts as a "config file" for the cluster setup.

# MIND_ROOT should be set in zshrc anyways.

# CLUSTER SETUP
cn_vm_name=FBS_client_yash

cn_vm_hostname=fbs-client
mn_vm_hostname=rs3labsrv5

cn_control_sshname=fbsc
cn_control_ip=192.168.122.4
cn_nic='ib0'
cn_mac='b8:3f:d2:ac:a0:06'
cn_data_ip='10.10.10.201'

mn_control_sshname=epfl5
mn_control_ip='10.90.46.65'
mn_nic='ibp49s0f0'
mn_mac='e8:eb:d3:36:f3:7e'
mn_data_ip='10.10.10.202'
mn_nic_numa=0

frontend_sshname='epfl4'
frontend_ip=192.168.122.1

using_roce='false'
data_subnet_prefix='24'
