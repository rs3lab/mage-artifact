# Scalable Far Memory: Balancing Faults and Evictions

## Overview
*Mage* is a remote memory system that enables offloading of scalable memory-intensive applications.
*Mage* is built on three principles to remove the system software overhead and achieve near hardware line-rate performance. The three principles are: 1. **Asynchronous decoupling** where *Mage* dedicates cores for eviction and disallows synchronous reclamation. 2. **Pipelined execution** where *Mage* splits one batch in reclamation into multiple and executes different stages in eviction in an out-of-order fashion with multiple batches. 3. **Contention avoidance** *Mage* prioritizes scalability over locality in its data structure design.
This repo includes the source code and documentation of *Mage*. Its organization is listed below.
```bash
Mage
    |---- mage-kernel    ; Source code of the mage implementation in Linux kernel
    |     |---- README.md   ; Instructions about how to install and run mage-kernel
    |     |---- Source Code
    |
    |---- mage-libos   ; Source code of the mage implementation in LibOS (OSv unikernel)
    |     |---- README.md   ; Instructions about how to install and run mage-libos
    |     |---- Source Code
    |
    |---- dilos   ; Source code of the DiLOS baseline used in Mage paper
    |---- hermit  ; Source code of Hermit baseline used in Mage paper
    |
    |---- README.md    ; General README
```

## Ready Environment
We have already set up a ready environment on our RS3Lab server. Please use EPFL VPN to access it. For the evaluation of `Mage-LibOS`, `Mage-Kernel`, and `DiLOS` we use rs3labsrv4 as the client node and rs3labsrv5 as the memory node. For the evaluation of `Hermit` we use rs3labsrv5 as the client node and rs3labsrv4 as the server node. The reason of this setting is that this way eliminates the time spent on switching kernel between experiments.
We will detailed info of EPFL VPN very soon.

## Requirement
### Hardware
- Server with Intel x86_64 CPUs
- Mellanox Connectx-5 NICs or above; Or BlueField 2/3 DPUs
### Software
- OS Distribution: Ubuntu 22.04 LTS
- Kernel version (For `Mage-LibOS` and `DiLOS`): 5.15.0-25-generic
- Compiler version: `gcc version 9.5.0 (Ubuntu 9.5.0-1ubuntu1~22.04)`
- Mellanox OFED version: `MLNX_OFED_LINUX-5.9-0.5.6.0-ubuntu22.04-x86_64`

## Installation:
### Mage-LibOS
Check the `Installation` section in the `README.md` of `mage-libos` subdir
### Mage-Kernel
Check the `Installation` section in the `README.md` of `mage-libos` subdir

## Evaluation:
### Mage-LibOS
Check the `Evaluation` section in the `README.md` of `mage-libos` subdir
### Mage-Kernel
Check the `Evaluation` section in the `README.md` of `mage-kernel` subdir

## Contact
If you have any questions or suggestions:
- About `mage-libos`, contact Yueyang Pan ([yueyang.pan@epfl.ch](mailto:yueyang.pan@epfl.ch))
- About `mage-kernel`, contact Yash Lala ([yash.lala@yale.edu](mailto:yash.lala@yale.edu))