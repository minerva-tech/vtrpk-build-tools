#!/bin/sh

LOG=/tmp/httpd.log

DEVICE=/dev/mtd3

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

if [ -z $1 ]; then
	log "Error: no image file specified"
	exit 1
fi

if [ ! -f $1 ]; then
    log "Error: no $1 image file found"
    exit 2
fi

size=`ls -l $1 | awk '{print $5}'`

log "Erasing ${DEVICE} ..."

flash_eraseall ${DEVICE} || exit_error "FAILED" 3

page_size=`mtdinfo ${DEVICE} | awk '/Minimum/ {printf $5}'`

log "Writing firmware image ..."

str=`nandwrite -p -s 2048 ${DEVICE} $1 | tee -a ${LOG}`
if [ $? -ne 0 ]; then
	exit_error "FAILED" 4
fi
block_number=`echo "$str" | awk '{if (NR == 1) printf $5}'`

s=`expr "$size" "+" "$page_size" `
s=`expr "$s" "-" "1" `
number_of_pages=`expr "$s" "/" "$page_size" `

dec_2_hex $number_of_pages

number_of_pages="$x0 $x1 $x2 $x3"

block_size=`mtdinfo ${DEVICE} | awk '/Eraseblock/ {printf $3}'`

# block_number += offset /* 24 UBL blocks + 32Mb / block_size */
offset=`expr "33554432" "/" "$block_size"`
offset=`expr "$offset" "+" "24"`
block_number=`expr "$offset" + "$block_number"`

dec_2_hex $block_number

block_number="$x0 $x1 $x2 $x3"

echo "00000000  66 ed ac a1 00 80 00 80  $number_of_pages $block_number" > /tmp/header.ascii
echo "00000010  01 00 00 00 00 80 00 80" >> /tmp/header.ascii

hexdump -R /tmp/header.ascii > /tmp/header.bin

log "Writing header ..."

nandwrite -p  ${DEVICE} /tmp/header.bin | tee -a ${LOG} || exit_error "FAILED" 5
