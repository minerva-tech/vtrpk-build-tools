#!/bin/sh

print_red_msg ()
{
    echo -e "\033[0;31m"$1"\033[0m"
}

print_green_msg ()
{
    echo -e "\033[1;32m""$1""\033[0m"
}

if [ -z "$1" ]; then
    print_red_msg "Error: SD/MMC DEVICE is not specified"
    exit 1
fi

print_green_msg "Creating new partitions table..."
sudo /usr/sbin/sgdisk $1 -Z || exit 2
sudo /usr/sbin/sgdisk $1 -g || exit 2
sudo /usr/sbin/sgdisk $1 -a 256 --new=1:2M:+64M || exit 11

print_green_msg "Writing bootloaders..."
sudo Output/minerva_sdmmc/output/host/usr/bin/uflash -d $1 -p DM3XX -u bootloader/ubl_mmc.bin -b Output/minerva_sdmmc/output/images/u-boot.bin
