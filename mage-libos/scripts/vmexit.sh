#!/usr/bin/bash
FILENAME=test.log
STR="Please attach"
while true
do
    sleep 1
    tail -n 10 ${FILENAME} | grep "${STR}"
    retVal=$?
    if [ ${retVal} -eq 0 ]; then
        break
    fi
done
echo "start"
sudo perf kvm stat record -p $(pidof qemu-system-x86_64)
