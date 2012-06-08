#!/bin/sh

if ! [ -f "./build/grdfs" ]; then
    echo "Please type 'make' first."
    exit
fi

if ! [ -n "$1" ]; then
    ./build/grdfs
    exit
fi

PROFILER=${AMDAPPSDKROOT}/tools/AMDAPPProfiler-2.5/x86_64/sprofile
FILE=`basename $1`

mkdir -p profiling
cd profiling
for i in 1 2 3 4 5
do
    $PROFILER -o ./$FILE${i}.atp -t -T -w ../ ../build/grdfs $1
done
