#!/bin/sh

/bin/mount /dev/mmcblk0p1 /mnt/1 || exit 1

cd /mnt/1

[ ! -f Image.sha1 ] && exit 2
sha1sum Image.sha1 || exit 3

[ ! -f ubl_nand.bin ] && exit 4
sha1sum ubl_nand.bin.sha1 || exit 5

/usr/bin/fw_write -l bootloader -f ubl_nand.bin -v -a 20 -m a1aced00 || exit 6
/usr/bin/fw_write -l rescue -f Image -v -a 80008000 -m a1aced66 || exit 7

cd /
umount /mnt/1 || exit 8

cd /sys/class/leds/error
cat max_brightness > brightness
