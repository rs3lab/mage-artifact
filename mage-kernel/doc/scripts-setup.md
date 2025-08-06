# Manager Script Setup

We have provided a set of scripts to automate the Mage-Linux cluster
management process. Before beginning evaluations, we need to provide these
scripts with some more information about our cluster. 

## Double-Check: `$MIND_ROOT` variable

Set the environment variable `MIND_ROOT` to point to the root directory of
Mage-Linux tree (aka: the directory whose README starts with "Welcome to
Mage-Linux!"). 

Many automated scripts will use this environment variable. 
So we recommend something like: 

```
export MIND_ROOT="$HOME/mage/mage-kernel"
```

in your shell init file (`~/.bashrc`, `~/.profile`, etc). 

**Make sure that even incoming SSH processes have `$MIND_ROOT` set!**
You can check by running `ssh servername 'echo $MIND_ROOT'`; make sure it
prints the value you set in your shell config. 



## Initializing Cluster Config File

You can find Mage-Linux's main config file in `$MIND_ROOT/scripts/config.sh`. 

Initialize it as follows: 

- `cn_vm_name` := the LibVirt name of your Compute Node (CN) VM. (find this via
  `virsh list --all`). Mage-Linux uses this to automatically reboot your CN VM
  after running evals. 
- `frontend_sshname` := The "SSH Name" of the Frontend's machine, eg:
  "server01". (see next section: [SSH Setup](#ssh-setup)) for details. 
- `frontend_ip` := An IP address that the Compute Node can use to communicate
  with the Frontend. If you've followed the recommended setup (CN set up in a
  VM on the same server as the Frontend), then this will be the IP address of
  the host server's virtual bridge interface (`virbr0` default) for
  communicating with VMs. 
- `mn_nic_numa` := the NUMA node your Memory Node's NIC is attached to. 
  Used to improve memory placement and improve Memory Node server performance. 
  Eg: "0". 
- `using_roce` := Set this to 'true' if the data plane network uses RoCE as a
  link-layer transport protocol. Otherwise, set it to 'false' (Mage will use
  Infiniband instead). 
- `data_subnet_prefix` := The network prefix length used when configuring the
  Control Node and Memory Node's IP connection (see: `cn_data_ip` config var). 
  Example setting: '24' to set up a '10.10.10.201/24' network. 

The following config variables come in two types: `cn_*` for the Compute Node,
and `mn_*` for the Memory Node. We'll describe the CN config vars below, and
omit their MN analogues for brevity. 

- `cn_control_sshname` := The "SSH Name" of your Compute Node. (see next
  section: [SSH Setup](#ssh-setup)). eg: "compute-vm". 
- `cn_vm_hostname` := the hostname of your Compute Node (CN) VM. Should match
  the output of the `hostname` command. 
- `cn_control_ip` := an IP address that the Frontend can use to ping
  the Compute Node VM. Mage-Linux uses this to check if the VM has started
  successfully. TODO(yash) elaborate. 
- `cn_nic` := The interface name of the NIC attached to your CN VM. This should
  be an RDMA NIC, capable of quick data-path access. eg: 'ib0' for an
  Infiniband NIC. 
- `cn_mac` := The Ethernet MAC address of the NIC attached to your CN VM. This
  is only relevant if your system is using RDMA over Converged Ethernet (RoCE)
  as a link-layer transport protocol. If your network uses Infiniband for
  transport, ignore this variable. 
  Example setting: 'b8:3f:d2:ac:a0:06'. 
- `cn_data_ip` := The IP address of the NIC attached to your CN VM. When your
  CN and MN try to connect to each other, they need to know each others' IP
  address; they use this "slow" TCP-IP connection to set up the "fast" RDMA
  connections needed for remote memory access. 
  Assign your interface an IP address that makes sense. Eg: '10.10.10.201'. 
  _This is needed even for Infiniband networks! We use IPoIB!_. 

**Please set all of these variables on all devices within the cluster!**
The contents of this file should be identical on all copies of `config.sh`, or
Mind-Linux gets confused. 


## SSH Setup

Mage-Linux's control scripts use SSH to perform actions on different nodes. 
Eg: before running a test, the `manager` script will recompile the MN server. 

_So you must set up passwordless SSH from all nodes to all nodes!_
Eg: a script run on the Frontend should be able to SSH to the Memory Node via
the command `ssh $mn_control_sshname` (see above). 
This SSH connection should hop to the MN's VM Host, then SSH into the MN's VM. 

You'll need to set this up using SSH aliases. 

First, create a SSH key without a password, and set up passwordless login on
all nodes using that key. [Tutorial](https://askubuntu.com/a/46935/1960051). 

Then, on the Frontend's machine, add _aliases_ to your SSH config file,
following this example: 

```
# in ~/.ssh/config

Host memory_node_vmhost
        User john_doe
        Hostname vm_host_1
        IdentityFile ~/.ssh/my_passwordless_ssh_key

host memory_node_vm
        User john_doe_vm_account
        Hostname 192.168.122.50
        
        # jump to `memory_node_vmhost` before trying to connect to
        # `memory_node_vm`. This facilitates easy connections even 
        # when the memory node may not be directly accessible from 
        # the Frontend. 
        ProxyJump memory_node_vmhost
        
        # no password required to log in
        IdentityFile ~/.ssh/my_passwordless_ssh_key
        Stricthostkeychecking no
```

Now, you can run `ssh memory_node_vm` and automatically hop to the MN VM!
After setting up this alias on all nodes (to be safe), you can set the config
option: 

```
# in config.sh
mn_control_sshname='memory_node_vm'
```

and Mage-Linux will automatically use it. 


## Installing Helper Scripts

Some helper scripts need to be installed to the user's PATH. 
This allows Mage-Linux to provide helpful commands to the cluster
administrator (eg: `setup-network`). 
These commands will "just do the right thing", regardless of where they're
installed, or on what machine (they use `$MIND_ROOT` variable). 
So you can run `setup-network` on the CN VM, MN VM, or the Frontend VM-host. 

- On the CN VM, please install all scripts in `$MIND_ROOT/scripts/cn/*` to the
  PATH. 
- On the MN VM, please install all scripts in `$MIND_ROOT/scripts/mn/*` to the
  PATH. 
- On the Frontend machine (which should be the host machine of the CN VM),
  please install all scripts in `$MIND_ROOT/scripts/vmhost/*` to the
  PATH. 

You can do this via a snippet like: 

```
# if ~/bin/ is in your PATH, l
for script in $MIND_ROOT/scripts/cn/*; do 
  ln -s $script ~/bin/
done
```

Please don't skip this step. 


## Checking Script Setup

- Run `manager all sync` on the Frontend's machine. If the Mage control path is
  set up correctly (and all VMs are running), the `manager` program should
  connect to all servers and make sure their code is up to date. 
- Run `manager all build` on the Frontend's machine. 
  The script should rebuild the Memory Node server, Frontend, and Compute Node
  kernel. 
- Run `manager cn install` on the Frontend's machine. 
  The script should rebuild the Compute Node (cn) kernel, and automatically
  install the new kernel to the VM. 
- Run `reset-fbs`. 
  The script should automatically restart the CN VM, then set up the Mage
  cluster. 
