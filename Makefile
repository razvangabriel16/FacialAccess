CC = aarch64-linux-gnu-gcc
CFLAGS = -g -Wall -Wextra -g #CFLAGS = -Wall -O2 -march=armv6 -mfpu=vfp -mfloat-abi=hard

C_SRCS = findcamera.c main.c
S_SRCS = filter1.s filter2.s filter3.s

COBJS = $(C_SRCS:.c=.o)
SOBJS = $(S_SRCS:.s=.o)

OBJS = $(COBJS) $(SOBJS)

TARGET = detection_system

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
