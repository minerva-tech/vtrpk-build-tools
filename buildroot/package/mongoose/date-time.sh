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

log "Setting ${1}.${2}.${3} ${4}:${5}:${6} ..."
date -s ${3}.${2}.${1}-${4}:${5}:${6} || exit_error "Failed to set system time & date" 1

log "Synchronizing rtc with system timer ..."
hwclock -w -f /dev/rtc0 || exit_error "Failed to syncronize " 2

log "Waiting for 2 secons ..."
sleep 2

log "Reading rtc ..."
hwclock -r /dev/rtc0 >> ${LOG} || exit_error "Failed to read rtc" 3
