CC = aarch64-linux-gnu-gcc
CFLAGS = -g -Wall -Wextra -fPIC
LDFLAGS = -shared
LIBNAME = libfilter4.so

C_SRCS = findcamera.c main.c
S_SRCS = filter1.s filter2.s filter3.s

PYS_SRCS = filter4.s

COBJS = $(C_SRCS:.c=.o)
SOBJS = $(S_SRCS:.s=.o)
PYSOBJS = $(PYS_SRCS:.s=.o)

OBJS = $(COBJS) $(SOBJS) $(PYSOBJS)

TARGET = detection_system

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm -lpthread

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBNAME): $(PYSOBJS)
	$(CC) $(LDFLAGS) -o $@ $^

shared: $(LIBNAME)

clean:
	rm -f $(OBJS) $(TARGET) $(LIBNAME)

.PHONY: all clean shared
