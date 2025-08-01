#!/bin/bash -e

if [[ $# -ne 1 ]]; then
  echo "$0 <cmd>"
  exit 1
fi

if [[ "$1" = "build" ]]; then
  echo 'Removing kernel logs and `.cache.mk`'
  rm -f /var/log/kern.log /var/log/syslog
  rm -f .cache.mk

  echo "Building the kernel"
  make bzImage -j $(( $(nproc --all) + 1 ))
  if [[ $? -ne 0 ]]; then
    echo "Error building kernel!"
    exit 1
  fi
elif [[ "$1" = "install" ]]; then
  echo "Installing the kernel"
  make -j $(( $(nproc --all) + 1 )) install
  if [[ $? -ne 0 ]]; then
    echo "Error installing kernel!"
    exit 1
  fi
else
  echo "unknown command"
  exit 1
fi
