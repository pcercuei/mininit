
CROSS_COMPILE ?= mipsel-linux-
CC = $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -O2 -mips32 -mtune=mips32r2 -fomit-frame-pointer
LDFLAGS = -s -static

TARGET = mininit

OBJS = loop.o init.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	-rm -f $(OBJS) $(TARGET)
