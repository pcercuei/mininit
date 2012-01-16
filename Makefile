
CROSS_COMPILE ?= mipsel-linux-
CC = $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -O2 -mips32 -mtune=mips32r2 -fomit-frame-pointer
LDFLAGS = -s -static

MININIT = mininit
SPLASHKILL = splashkill

M_OBJS = loop.o init.o
S_OBJS = splashkill.o

$(MININIT): $(M_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(M_OBJS) $(LIBS)

$(SPLASHKILL): $(S_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(S_OBJS) $(LIBS)

all: $(MININIT) $(SPLASHKILL)

clean:
	-rm -f $(M_OBJS) $(S_OBJS) $(MININIT) $(SPLASHKILL)
