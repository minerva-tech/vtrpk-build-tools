#!/bin/sh

rm -fR output

cp -f configs/vrtpk_0_defconfig .config
make oldconfig

make || exit 1

cp -f configs/vrtpk_defconfig .config
make oldconfig

make || exit 1
