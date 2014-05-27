#!/bin/sh

/usr/bin/killall teplovisor

/usr/bin/fpga.sh
/usr/bin/loadmodules_h264enc.sh
sleep 1

if [ -f /mnt/2/console ]; then
    cp -f /mnt/2/console /usr/bin/teplovisor
else
    cp -f /mnt/1/teplovisor_demo /usr/bin/teplovisor
fi

cd /mnt/2
/usr/bin/teplovisor 38400 0 > /dev/ttyS0
