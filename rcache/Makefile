TARGET := server

RR_COMMON_DIR := $(abspath $(CURDIR)/../libfs/lib/rdma)
CONF_COMMON_DIR := $(abspath $(CURDIR)/../libfs/lib/aconf)

LIBS = -L$(RR_COMMON_DIR) -L$(CONF_COMMON_DIR)
INCLUDES := $(addprefix -I, $(RR_COMMON_DIR) $(CONF_COMMON_DIR))

RM = rm -f  # rm command
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

CFLAGS = -g -O0 -std=gnu99
LDFLAGS = -lrdmacm -libverbs -lm -lrt -lrdma -laconf

.PHONY: clean

all: $(OBJS)
		$(CC) $(OBJS) $(CFLAGS) $(LIBS) $(LDFLAGS) -o $(TARGET)

%.o: %.c
		$(CC) $(INCLUDES) -c $(CFLAGS) -o $@ $<

clean:
		rm -f $(OBJS) $(TARGET)
