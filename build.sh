#!/bin/sh

for i in x-tools bootloader
do
    cd $i
    if [ ! -f .built]; then
	./build.sh || exit 1
	touch .built
    fi
    cd ../
done

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
    if [ -n "$2" -a -d ../${2}/output/host ]; then
	mkdir -p output
	cd output
	for i in host staging toolchain
	do
	    ln -s ../../${2}/output/${i} ${i}
	done
	cp -a ../../${2}/output/stamps stamps
	cd ../
    fi
    cp configs/${1}_defconfig .config
    make oldconfig || exit 1
    make || exit 1
    popd
    touch Output/${1}/.built
}

buildroot minerva_sdmmc
buildroot vrtpk
buildroot vrtvk
