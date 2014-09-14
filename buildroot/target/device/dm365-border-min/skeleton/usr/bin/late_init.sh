#!/bin/sh

sleep 2

/sbin/modprobe davinci && /sbin/modprobe g_cdc && /sbin/ifup -a

/sbin/modprobe mtdblock && /sbin/modprobe && /sbin/modprobe davinci_nand && /sbin/modprobe m25p80 && /sbin/modprobe davinci_spi

/etc/init.d/S50dropbear start
