#! /bin/bash

PATH=$PATH:.
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:../build/:../lib/rdma/" LD_PRELOAD="../build/libmlfs.so ../lib/rdma/librdma.so" ${@}

