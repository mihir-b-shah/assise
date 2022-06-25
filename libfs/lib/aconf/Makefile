CC = gcc
CFLAGS = -fPIC -O2 -g
LDFLAGS = -shared

RM = rm -f
TARGET_LIB = libaconf.so
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

CFLAGS += -DSANITY_CHECK
CFLAGS += -DDEBUG
CFLAGS += -D_GNU_SOURCE

LDLIBS = -lpthread

.PHONY: all
	all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
		$(CC) -g -o $@ $^ ${LDFLAGS} ${LDLIBS}

.PHONY: clean
clean:
	${RM} ${TARGET_LIB} ${OBJS}

