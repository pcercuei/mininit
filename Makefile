
CROSS_COMPILE ?= /opt/gcw0-toolchain/usr/bin/mipsel-gcw0-linux-uclibc-
CC = $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -O2 -fomit-frame-pointer
LDFLAGS = -s -static

MININIT = mininit
SPLASHKILL = splashkill

M_OBJS = loop.o init.o
S_OBJS = splashkill.o

.PHONY: all clean

all: $(MININIT) $(SPLASHKILL)

clean:
	rm -f $(M_OBJS) $(S_OBJS) $(MININIT) $(SPLASHKILL)

$(MININIT): $(M_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(M_OBJS) $(LIBS)

$(SPLASHKILL): $(S_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(S_OBJS) $(LIBS)
