#!/bin/sh

PREFIX=${PWD}/../buildroot-min/output

export PATH=$PATH:${PREFIX}/host/usr/bin

echo $PATH

make clean
make CROSS_COMPILE=arm-linux- KERNEL_DIR=${PREFIX}/build/linux-custom
make INSTALL_DIR=${PREFIX}/target/usr/bin install
