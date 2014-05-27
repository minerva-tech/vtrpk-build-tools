#!/bin/sh

TARGET=arm-none-eabi
HOST=`gcc -dumpmachine`

export PATH=${PATH}:${PWD}/install/bin

tar xjvf dl/binutils-*

cd binutils-*
./configure --target=${TARGET} --host=${HOST} --build=${HOST} --prefix=${PWD}/../install
make
make install
cd ../

tar xzvf dl/newlib-*
cd newlib-*
cp ../patches/newlib.patch .
patch -p1 -i newlib.patch
cd ../

tar xjvf dl/gcc-*
cd gcc-*
#cp -f ../patches/t-arm-elf gcc/config/arm/
sed -e "s/^host_subdir.*/host_subdir = host-${HOST}/" -i libgcc/Makefile.in 
./configure --target=${TARGET} --host=${HOST} --build=${HOST} --prefix=${PWD}/../install --enable-languages="c,c++" --disable-libssp --with-newlib --with-headers=${PWD}/../newlib-2.1.0/newlib/libc/include
make all-gcc
make install-gcc
cd ../install/bin
ln -s ${TARGET}-gcc ${TARGET}-cc
cd ../../

cd newlib-*
./configure --target=${TARGET} --host=${HOST} --build=${HOST} --prefix=${PWD}/../install
make
make install
cd ../

cd gcc-*
make
make install
cd ../
