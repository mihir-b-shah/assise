CC = gcc

C_SRCS = $(wildcard *.c)
TGTS = $(C_SRCS:.c=.run)
CC_SRCS = $(wildcard *.cc)
MISC_TGTS = $(CC_SRCS:.cc=.exe)

CUR_DIR = $(shell pwd)
LIBFS_DIR = $(CUR_DIR)/../build/
RDMA_DIR = $(CUR_DIR)/../lib/rdma/
INCLUDES= -I$(LIBFS_DIR)/../src -I$(RDMA_DIR)

CFLAGS=$(INCLUDES) -std=gnu99 -O2

LDFLAGS = -Wl,-rpath=$(abspath $(LIBFS_DIR)) \
					-Wl,-rpath=$(abspath $(LIBFS_DIR)) \
			-lpthread -lrt -lm -std=gnu99

all: $(MISC_TGTS) $(TGTS)

%.exe: %.cc
	$(CC) -g -o $@ $^ -lpthread -lrt -lm

%.run: %.c
	$(CC) -g -o $@ $^ -L$(LIBFS_DIR) -lmlfs -L$(RDMA_DIR) -lrdma -DMLFS $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf *.o $(TGTS) $(MISC_TGTS)

