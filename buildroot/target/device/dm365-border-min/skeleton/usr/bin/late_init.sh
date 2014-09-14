#!/bin/sh

sleep 2

/sbin/modprobe davinci_nand

/sbin/modprobe davinci && /sbin/modprobe g_cdc && /sbin/ifup -a

/etc/init.d/S50dropbear start
