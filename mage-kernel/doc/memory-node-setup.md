# Memory Node Setup Instructions

The Mage-Linux memory server runs in userspace. It can be run on bare metal or
inside a virtual machine; we recommend using a VM for simpler setup. 

## Hardware Requirements

See the [main README](../README.md). 

## Software Requirements

- Base OS: we tested Mage-Linux with Ubuntu Server 18.04 LTS (of which the
  default kernel is 4.15.x). 
- NIC Driver Version (Mellanox OFED): MLNX_OFED_LINUX-5.8-5.1.1.2 (OFED-5.8-5.1.1)
- The `chronic` command (`apt install moreutils`)

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

Make sure your user can run `sudo` commands without a password. 
If your username is "john", you can do this by appending the following line to
/etc/sudoers: 
```
john ALL=(ALL) NOPASSWD:ALL
```
to the end of /etc/sudoers file. 

This is insecure, so we recommend running Mage evaluations in a controlled
environment. Please remove this line when evaluations are complete. 

### OFED Driver Setup


Install Mellanox OFED. OFED provides network drivers that Mage uses to get the
best performance out of its RDMA NIC. Unfortunately, it is a third-party
library by Mellanox (now acquired by NVIDIA). 

1. First, download MLNX_OFED_LINUX-5.8-5.1.1.2 from NVIDIA's
   [website](https://downloaders.azurewebsites.net/downloaders/mlnx_ofed_downloader/downloader3.html).
   You can find the correct version under the "Archives" tab. 
2. Install OFED with
   `sudo ./mlnxofedinstall`. 

Reboot the VM. You should see "ofed" printed in the kernel's log after boot
(check via `dmesg | grep -i ofed`). 

### Memory Server Setup

If you've installed the dependencies correctly, the memory server should build
itself when Mage-Linux is first run. 

You can also build it manually via:

```
cd mem-server
make
```
