#!/bin/bash
clear
nrdpus=2
while [ $nrdpus -le 64 ]
do
dpu-upmem-dpurte-clang -DNR_TASKLETS=16 -DNRDPU=$nrdpus  -DSTACK_SIZE_DEFAULT=2048 -O3 SHA256DPU.c -o SHA256DPU
gcc --std=c11 -DNR_TASKLETS=16 -DNRDPU=$nrdpus SHA256hostWEAK.c -o SHA256hostWEAK `dpu-pkg-config --cflags --libs dpu`
./SHA256hostWEAK
nrdpus=$(( $nrdpus * 2 ))
done
