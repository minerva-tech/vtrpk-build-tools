#!/bin/sh

sleep 2

/sbin/modprobe mtdblock

/sbin/modprobe m25p80

#/sbin/modprobe davinci_nand

/sbin/modprobe davinci && /sbin/modprobe g_ether && /sbin/ifup -a

/usr/sbin/udhcpd

/etc/init.d/S50dropbear start
