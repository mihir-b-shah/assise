CC = gcc
CFLAGS = -fPIC -O0 -g
LDFLAGS = -shared

RM = rm -f
TARGET_LIB = librrdma.so
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

CFLAGS += -DSANITY_CHECK
CFLAGS += -DDEBUG
CFLAGS += -DIBV_RATE_LIMITER
CFLAGS += -DIBV_WRAPPER_INLINE
CFLAGS += -D_GNU_SOURCE

LDLIBS = -lrdmacm -libverbs -lpthread -lrt

.PHONY: all
	all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
		$(CC) -g -o $@ $^ ${LDFLAGS} ${LDLIBS}

.PHONY: clean
clean:
	${RM} ${TARGET_LIB} ${OBJS}

