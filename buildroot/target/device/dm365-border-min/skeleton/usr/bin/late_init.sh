#!/bin/sh

sleep 1

/sbin/modprobe mtdblock && /sbin/modprobe davinci_nand && /sbin/modprobe m25p80 && /sbin/modprobe at25 && /sbin/modprobe davinci_spi

/sbin/modprobe davinci && /sbin/modprobe g_cdc && /sbin/ifup -a

/etc/init.d/S50dropbear start
