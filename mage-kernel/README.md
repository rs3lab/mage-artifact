# `Mage-Linux` 

Welcome to Mage-Linux! Please contact Yash Lala <yash.lala@yale.edu> if you
have any questions or suggestions about this software component. 

## System Overview

Mage-Linux is comprised of three components: 

1. The Compute Node (CN): a VM running a modified Linux kernel. This kernel runs
   applications, and offloads their far memory to...
2. The Memory Node (MN): a VM running a userspace daemon. The daemon acts as a
   pool of far-memory; the Compute Node places pages on the Memory Node
   when the Compute Node runs out of local memory. 
3. The "Frontend": a userspace process that coordinates memory
   allocations between the Compute and Memory Nodes. The frontend
   is invoked at allocation time only; it does not take part in the
   far-memory data path. 
   The frontend is just an implementation detail of our system; in
   principle, all of its features can be integrated into the client. 
   Much of the frontend controller logic is focused on handling the
   "Multiple-Compute-Node" case; this isn't required for our SOSP paper's
   evaluations. 

Directory structure: 

1. `mind_linux` := kernel for the Compute Node. 
2. `mem-server` := the userspace daemon on the Memory Node. 
3. `frontend` := The "frontend" daemon. 
4. `apps` := contains source code (and evaluation scripts) for the workloads
   we examined in our paper. 
5. `doc` := documentation
6. `scripts` := contains helper scripts to make the evaluation process
   simpler. 


## Installation

**For SOSP Evaluators**: we have set up a Mage environment on the RS3Lab
servers, ready for you to evaluate! (see top-level README for details). 
You are also welcome to re-install Mage components to verify correctness. 

The installation procedure contains 5 steps: 

1. [Set up the Cluster VMs](doc/cluster-vm-setup.md)
2. [Set up the Compute Node](doc/compute-node-setup.md). 
3. [Set up the Memory Node](doc/memory-node-setup.md). 
4. [Set up the Frontend](doc/frontend-setup.md). 
5. [Set up Manager Scripts](doc/scripts-setup.md). 


## Running Programs

After setting

- [Reproducing Paper Evaluations](doc/reproducing-paper-evals.md)
