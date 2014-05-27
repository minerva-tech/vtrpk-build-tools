#!/bin/sh

NET=enp0s19f2u1

rm -f /home/sand/.ssh/known_hosts

sudo /bin/ifconfig ${NET} down
sudo /bin/ip link set ${NET} address 02:11:22:33:44:88
sudo /bin/ifconfig ${NET} 192.168.11.1 up

scp -i $HOME/.ssh/teplovisor_id_rsa root@192.168.11.111:/teplovisor_ccdc_prv_rsz.yuv captured.yuv
