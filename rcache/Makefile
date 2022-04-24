TARGET := server

RR_COMMON_DIR := $(abspath $(CURDIR)/../libfs/lib/rdma)
CONF_COMMON_DIR := $(abspath $(CURDIR)/../libfs/lib/aconf)

LIBS = -L$(RR_COMMON_DIR) -L$(CONF_COMMON_DIR)
INCLUDES := $(addprefix -I, $(RR_COMMON_DIR) $(CONF_COMMON_DIR))

RM = rm -f
C_SRCS = $(wildcard *.c)
CC_SRCS = $(wildcard *.cc)
C_OBJS = $(C_SRCS:.c=.oc)
CC_OBJS = $(CC_SRCS:.cc=.ocx)

CFLAGS = -g -O0 -std=gnu99
CXXFLAGS = -g -O0 -std=c++11
LDFLAGS = -g -lrdmacm -libverbs -lm -lrt -lrdma -laconf -lstdc++

.PHONY: all clean

all: $(C_OBJS) $(CC_OBJS)
		$(CC) $(C_OBJS) $(CC_OBJS) $(CFLAGS) $(LIBS) $(LDFLAGS) -o $(TARGET)

%.oc: %.c
		$(CC) $(INCLUDES) -c $(CFLAGS) -o $@ $<

%.ocx: %.cc
		$(CXX) $(INCLUDES) -c $(CXXFLAGS) -o $@ $<

clean:
		rm -f $(C_OBJS) $(CC_OBJS) $(TARGET)
