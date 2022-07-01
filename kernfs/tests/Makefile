CC=gcc
CXX=g++ -std=c++11

#CC = clang-3.8
#CXX = clang++-3.8

########
#  Lib directories
########
LIBFS_DIR := $(CURDIR)/../../libfs
KERNFS_DIR := $(CURDIR)/../

INCLUDES  := $(addprefix -I, .. .)

LD_FLAGS = -lpthread -lm -lrt
LD_FLAGS_CXX = -lpthread -lrt
#DEBUG = -g -O0
#DEBUG = -g -Ofast
DEBUG = -g -O0

########
#  Phony targets
########
.PHONY: kernfs

BIN := kernfs

all: $(BIN)

clean:
	rm -rf *.o $(BIN)

kernfs: kernfs.c
	$(CC) $^ $(DEBUG) -o $@ $(INCLUDES) -L../build -lkernfs $(LD_FLAGS) -Wl,--no-whole-archive -luuid -lnuma 
