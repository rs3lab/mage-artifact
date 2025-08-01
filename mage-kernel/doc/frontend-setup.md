# Frontend Setup Instructions

The Frontend is stable. 
We recommend running it on the host machine of the Compute Node VM. 
(Remember not to use the same set of cores allocated to the VM!). 
See: Cluster Setup Instructions. 

## Hardware Requirements

See the [main README](../README.md). 

## Software Requirements

- Base OS: we tested Mage-Linux with Ubuntu Server 22.04 LTS. 
- The Frontend does not require OFED to be installed. 
- Thrift Stack (`apt install thrift-compiler libthrift-dev`). 
  This dependency will likely be removed in a future release. 
- Libevent and Hiredis (`apt install libevent-dev libhiredis-dev`). 
  This dependency will likely be removed in a future release. 
- C++ Boost Libraries (`apt install libboost-system-dev`)
  This dependency will likely be removed in a future release. 
- `chronic` command: (`apt install moreutils`). 

## Installation Instructions

### Repo Setup

Clone this repo into the VM. 

Set the environment variable `MIND_ROOT` to point to the root directory of
Mage-Linux tree (aka: the directory whose README starts with "Welcome to
Mage-Linux!"). 

Many automated scripts will use this environment variable. 
So we recommend something like: 

```
export MIND_ROOT="$HOME/mage/mage-linux"
```

in your shell init file (`~/.bashrc`, `~/.profile`, etc). 

**Make sure that even incoming SSH processes have `$MIND_ROOT` set!**
You can check by running `ssh servername 'echo $MIND_ROOT'`; make sure it
prints the value you set in your shell config. 

### User Setup

You will need to set up password-less `sudo` for a small set of commands. 
If your username is "john", we recommend appending the following line to
/etc/sudoers: 
```
john ALL=(ALL) NOPASSWD:/usr/bin/virsh, /usr/bin/mlxfwreset, /usr/bin/tee, /usr/bin/perf
```

### Frontend Setup

The frontend should build itself automatically when the system is first run. 

If it fails, you should be able to manually build it via: 

```sh
cd frontend
./build_mind_ctrl.sh
```
