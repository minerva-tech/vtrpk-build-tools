#!/bin/sh

buildroot ()
{
    if [ ! -d Output/$1 ]; then
	mkdir -p Output/$1
	for i in board boot Config.in configs dl docs fs linux Makefile package support target toolchain
	do
	    ln -s ../../buildroot/$i Output/${1}/$i
	done
    fi
    pushd Output/$1
    cp -f configs/${1}_defconfig .config
    make oldconfig
    make
    popd
}

buildroot vrtpk
