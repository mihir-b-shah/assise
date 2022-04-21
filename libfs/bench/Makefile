CC = gcc

C_SRCS = $(wildcard *.c)
TGTS = $(C_SRCS:.c=.run)

CUR_DIR = $(shell pwd)
LIBFS_DIR = $(CUR_DIR)/../build/
LIBFS=$(LIBFS_DIR)/libmlfs.a
INCLUDES= $(LIBFS_DIR)/../src

CFLAGS=-I$(INCLUDES) -std=gnu99

LDFLAGS = -Wl,-rpath=$(abspath $(LIBFS_DIR)) \
			-Wl,-rpath=$(abspath $(RDMA_DIR)) \
			-lpthread -lrt -lm -std=gnu99

all: setup $(TGTS)

setup:
	$(CC) -g -o setup setup.cc -lpthread -lrt -lm

%.run: %.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf *.o $(TGTS) setup

