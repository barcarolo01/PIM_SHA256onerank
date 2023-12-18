#!/bin/bash
clear
nrtask=1
while [ $nrtask -le 24 ]
do
dpu-upmem-dpurte-clang -DNR_TASKLETS=$nrtask -DNRDPU=1  -DSTACK_SIZE_DEFAULT=2048 -O3 SHA256DPU.c -o SHA256DPU
gcc --std=c11 -DNR_TASKLETS=$nrtask -DNRDPU=1 SHA256host.c -o SHA256host `dpu-pkg-config --cflags --libs dpu`
./SHA256host
nrtask=$(( $nrtask + 1 ))
done
