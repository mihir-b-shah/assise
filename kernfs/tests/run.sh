#!/bin/bash

PATH=$PATH:.
#LD_LIBRARY_PATH=../build:../../libfs/lib/libspdk/libspdk/:../../libfs/lib/nvml/src/nondebug/ MLFS_PROFILE=1 taskset -c 0,7 $@ 
#LD_LIBRARY_PATH=../build:../../libfs/lib/libspdk/libspdk/:../../libfs/lib/nvml/src/nondebug/ taskset -c 0,7 $@ 
#LD_LIBRARY_PATH=../build:../../libfs/lib/libspdk/libspdk/:../../libfs/lib/nvml/src/nondebug/ $@ 
#LD_LIBRARY_PATH=../build:../../libfs/lib/libspdk/libspdk/:../../libfs/lib/nvml/src/nondebug/ MLFS_PROFILE=1 taskset -c 0,7  $@ 
#LD_LIBRARY_PATH=../build:../../libfs/lib/libspdk/libspdk/:../../libfs/lib/nvml/src/nondebug/ MLFS_PROFILE=1 taskset -c 8,15  $@ 
#LD_LIBRARY_PATH=../build:../../libfs/lib/libspdk/libspdk/:../../libfs/lib/nvml/src/nondebug/ MLFS_PROFILE=1 $@ 
#LD_LIBRARY_PATH=../build:../../libfs/lib/libspdk/libspdk/:../../libfs/lib/nvml/src/nondebug/ MLFS_PROFILE=1 taskset -c 8,15 $@ 
#LD_LIBRARY_PATH=../build:../../libfs/lib/libspdk/libspdk/:../../libfs/lib/nvml/src/nondebug/ MLFS_PROFILE=1 $@ 
#LD_LIBRARY_PATH=../build:../../libfs/lib/libspdk/libspdk/:../../libfs/lib/nvml/src/nondebug/ LD_PRELOAD=../../libfs/lib/jemalloc-4.5.0/lib/libjemalloc.so.2 taskset -c 8,15 $@ 
#LD_LIBRARY_PATH=../build:../../libfs/lib/libspdk/libspdk/:../../libfs/lib/nvml/src/nondebug/ LD_PRELOAD=../../libfs/lib/jemalloc-4.5.0/lib/libjemalloc.so.2 taskset -c 8,15 $@ 
#LD_LIBRARY_PATH=../build:../../libfs/lib/nvml/src/nondebug/ LD_PRELOAD=../../libfs/lib/jemalloc-4.5.0/lib/libjemalloc.so.2 numactl -N0 -m0 $@
LD_LIBRARY_PATH=../build:/opt/apps/gcc/9.1.0/lib64 MLFS_PROFILE=1 numactl -N0 -m0 $@ 
#LD_LIBRARY_PATH=../build:/opt/apps/gcc/9.1.0/lib64 MLFS_PROFILE=1 gdb numactl
#LD_LIBRARY_PATH=../build:../../libfs/lib/libspdk/libspdk/:../../libfs/lib/nvml/src/nondebug/ LD_PRELOAD=../../libfs/lib/jemalloc-4.5.0/lib/libjemalloc.so.2 MLFS_PROFILE=1 taskset -c 0,7 $@ 
