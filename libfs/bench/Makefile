CC = gcc

C_SRCS = $(wildcard *.c)
TGTS = $(C_SRCS:.c=.run)
CC_SRCS = $(wildcard *.cc)
MISC_TGTS = $(CC_SRCS:.cc=.exe)

CUR_DIR = $(shell pwd)
LIBFS_DIR = $(CUR_DIR)/../build/
LIBFS=$(LIBFS_DIR)/libmlfs.a
INCLUDES= $(LIBFS_DIR)/../src

CFLAGS=-I$(INCLUDES) -std=gnu99

LDFLAGS = -Wl,-rpath=$(abspath $(LIBFS_DIR)) \
			-Wl,-rpath=$(abspath $(RDMA_DIR)) \
			-lpthread -lrt -lm -std=gnu99

all: $(MISC_TGTS) $(TGTS)

%.exe: %.cc
	$(CC) -g -o $@ $^ -lpthread -lrt -lm

%.run: %.c
	$(CC) -g -o $@ $^ -I$(INCLUDES) -L$(LIBFS_DIR) -lmlfs -DMLFS $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf *.o $(TGTS) $(MISC_TGTS)

