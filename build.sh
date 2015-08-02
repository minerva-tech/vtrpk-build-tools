#!/bin/sh

buildroot ()
{
    [ -f Output/${1}/.built ] && return
    if [ ! -d Output/$1 ]; then
	mkdir -p Output/$1
	for i in board boot Config.in configs dl docs fs linux Makefile package support target toolchain
	do
	    ln -s ../../buildroot/$i Output/${1}/$i
	done
    fi
    pushd Output/$1
    cp configs/${1}_defconfig .config
    make oldconfig
    make || exit 1
    popd
    touch Output/${1}/.built
}

buildroot minerva_sdmmc
