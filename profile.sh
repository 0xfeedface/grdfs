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
$PROFILER -o ./$FILE.atp -t -T -w ../ ../build/grdfs $1
