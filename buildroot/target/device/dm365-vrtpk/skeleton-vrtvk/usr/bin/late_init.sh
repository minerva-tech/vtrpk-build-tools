#!/bin/sh

sleep 2

/sbin/modprobe mtdblock

/sbin/modprobe m25p80

#/sbin/modprobe davinci_nand

/sbin/modprobe davinci || exit 1

/sbin/modprobe -a nls_utf8 nls_ascii nls_base nls_cp855 nls_cp866 nls_iso8859-1 nls_iso8859-5 nls_koi8-r vfat

/sbin/modprobe -a sd_mod usb-storage

/sbin/modprobe g_ether && /sbin/ifup -a && /usr/sbin/udhcpd && /etc/init.d/S50dropbear start
