TARGET := server

RDMA_COMMON_DIR := $(abspath $(CURDIR)/../libfs/lib/rdma)

LIBS = -L$(RDMA_COMMON_DIR)
INCLUDES := $(addprefix -I, $(RDMA_COMMON_DIR))

RM = rm -f  # rm command
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

CFLAGS = -g -O2
LDFLAGS = -lrdmacm -libverbs -lrdma -lm -lrt

.PHONY: clean

all: $(OBJS)
		$(CC) $(OBJS) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $(TARGET)

%.o: %.c
		$(CC) $(INCLUDES) -c $(CFLAGS) -o $@ $<

clean:
		rm -f $(OBJS) $(BIN)

test:
		@LD_LIBRARY_PATH=$(RDMA_COMMON_DIR) ./$(TARGET)

