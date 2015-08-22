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

/usr/bin/fw_write -l kernel -f $1 -a 80008000 -m a1aced66 -v | tee -a ${LOG} || exit_error "FAILED" 1
