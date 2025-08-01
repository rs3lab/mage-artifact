# Cluster VM Setup

**Mage is a research prototype. The Compute Node Kernel may be unstable!** 
_We
recommend installing the Compute Node in a virtual machine_. 

## Recommended Cluster

Suppose you have two physical servers, S1 and S2, where both machines are
connected via a ConnectX-5 or above (ideally a Bluefield-2 DPU) NIC. 
We recommend a VM setup as follows: 

- Set up the Compute Node in a VM on S1. 
- Set up the Frontend on S1, on bare metal. (make sure the allocated CPUs don't
  overlap w/ those assigned to the VM). 
- Set up the Memory Node in a VM on S2. 

_Make sure VM memory is backed by 1 GiB huge pages!_. This is needed to
minimize virtualization overhead and ensure high performance. 

## Hardware Requirements

See the [Main README](../README.md) for general guidelines. 

Other notes: 
- In our paper, we set the RDMA NIC to Infiniband mode. Mage-Linux can also be
  evaluated on RoCEv2 networks; see the [Script Setup](scripts-setup.md) docs
  to understand how. 
- We enabled IO-MMU and used PCIe passthrough to attach NIC to VMs.

## Software Requirements

We highly recommend managing the VMs via the LibVirt (`virsh`) interface. 
Our helper scripts call `virsh` directly when resetting system state. 

LibVirt can delegate to many different types of backing VM. 
We recommend using QEMU+KVM, using VFIO passthrough to give VMs access to the
NICs. 
