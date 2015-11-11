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

/sbin/modprobe davinci_nand

log "Writing firmware ..."

/bin/echo 81 > /sys/class/gpio/export
/bin/sleep 1
/bin/echo out > /sys/class/gpio/gpio81/direction
/bin/echo 1 > /sys/class/gpio/gpio81/value

/usr/bin/fw_write -l kernel -f $1 -a 80008000 -m a1aced66 -v | tee -a ${LOG} || exit_error "FAILED" 1

/sbin/rmmod davinci_nand

/bin/echo 0 > /sys/class/gpio/gpio81/value
/bin/echo 81 > /sys/class/gpio/unexport
