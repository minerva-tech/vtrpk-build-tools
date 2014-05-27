#!/bin/sh

echo 49 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio49/direction
echo 0 > /sys/class/gpio/gpio49/value

echo 92 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio92/direction
echo 1 > /sys/class/gpio/gpio92/value
echo 0 > /sys/class/gpio/gpio92/value
sleep 1
echo 1 > /sys/class/gpio/gpio92/value
