TARGET := client

RDMA_COMMON_DIR := $(abspath $(CURDIR)/../../libfs/lib/rdma)
RR_COMMON_DIR := $(abspath $(CURDIR)/../rr)

LIBS = -L$(RDMA_COMMON_DIR) -L$(RR_COMMON_DIR)
INCLUDES := $(addprefix -I, $(RDMA_COMMON_DIR) $(RR_COMMON_DIR))

RM = rm -f  # rm command
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

CFLAGS = -g -O2 -std=gnu99
LDFLAGS = -lrdmacm -libverbs -lrdma -lm -lrt -lrr

.PHONY: clean

all: $(OBJS)
		$(CC) $(OBJS) $(CFLAGS) $(LIBS) $(LDFLAGS) -o $(TARGET)

%.o: %.c
		$(CC) $(INCLUDES) -c $(CFLAGS) -o $@ $<

clean:
		rm -f $(OBJS) $(TARGET)

test:
		@LD_LIBRARY_PATH=$(RDMA_COMMON_DIR) ./$(TARGET)

