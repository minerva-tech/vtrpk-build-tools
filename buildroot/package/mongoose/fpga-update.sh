#!/bin/sh

LOG=/tmp/httpd.log

log()
{
    echo "<br><b>$1</b>" >> ${LOG}
    echo "$1"
}

exit_error()
{
    echo "<br><b>Error: $1 </b><br>" >> ${LOG}
    exit    $2
}

rm -f ${LOG}

if [ -z $1 ]; then
	log "Error: no image file specified"
	exit 1
fi

if [ ! -f $1 ]; then
    log "Error: no $1 image file found"
    exit 2
fi

log "Erasing /dev/mtd0 ..."

/usr/sbin/flash_erase /dev/mtd0 2097152 0 || exit_error "FAILED" 3

log "Writing FPGA image ..."

/bin/dd if=$1 seek=32 bs=65536 of=/dev/mtdblock0 || exit_error "FAILED" 4
