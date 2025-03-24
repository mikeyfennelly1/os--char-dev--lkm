#!/bin/bash

make clean
cd src
make clean
cd ..
make
cd src
make clean
cd ..
sudo rmmod sysinfo
sudo insmod ./build/sysinfo.ko
sudo chmod 0666 /dev/sysinfo