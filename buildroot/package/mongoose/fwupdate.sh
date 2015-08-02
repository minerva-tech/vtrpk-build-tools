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

dec_2_hex()
{
	n=`expr "$1" "/" "256"`
	k=`expr "$n" "*" "256"`
	x=`expr "$1" "-" "$k"`
	x0=`printf %02x $x`
	
	m=`expr "$n" "/" "256"`
	k=`expr "$m" "*" "256"`
	x=`expr "$n" "-" "$k"`
	x1=`printf %02x $x`
	
	n=`expr "$m" "/" "256"`
	k=`expr "$n" "*" "256"`
	x=`expr "$m" "-" "$k"`
	x2=`printf %02x $x`
	
	m=`expr "$n" "/" "256"`
	k=`expr "$m" "*" "256"`
	x=`expr "$n" "-" "$k"`
	x3=`printf %02x $x`
}

rm -f ${LOG}

log "Writing firmware ..."

/usr/bin/fw_write -l kernel -f $1 -a 80008000 -m a1aced66 -v | tee -a ${LOG} || exit_error "FAILED" 1
