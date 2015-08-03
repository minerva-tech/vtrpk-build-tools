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

if [ -z "$2" ]; then
    print_red_msg "Error: platform is not specified"
    exit 1
fi

print_green_msg "Writing bootloaders..."
sudo Output/minerva_sdmmc/output/host/usr/bin/uflash -d $1 -p DM3XX -u bootloader/ubl_mmc.bin -b Output/minerva_sdmmc/output/images/u-boot.bin || exit 5

