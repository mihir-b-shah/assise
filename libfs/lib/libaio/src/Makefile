CC = gcc
CFLAGS = -fPIC -O2
LDFLAGS = -shared
TARGET = libaio.so  # target lib

SRCS = $(wildcard *.c)
HEADERS = $(wildcard *.h)
OBJS = $(SRCS:.c=.o)

all: ${TARGET}

$(TARGET): $(OBJS)
	$(CC) $(OBJS) ${LDFLAGS} -o $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<  -o $@
