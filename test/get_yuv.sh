##!/usr/bin/expect -f
#!/bin/sh

#NET=enp0s19f2u1

#rm -f /home/sand/.ssh/known_hosts

#sudo /bin/ifconfig ${NET} down
#sudo /bin/ip link set ${NET} address 02:11:22:33:44:88
#sudo /bin/ifconfig ${NET} 192.168.11.1 up

#spawn scp root@192.168.11.11:/home/default/teplovisor_ccdc_output.yuv captured.yuv
#expect "assword:"
#send -- "qwerty\r"
#expect eof

scp  root@192.168.11.111:/home/default/teplovisor_ccdc_output.yuv captured.yuv
