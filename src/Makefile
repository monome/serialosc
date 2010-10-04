CC = gcc
LD = $(CC)

CFLAGS = -I./private
LDFLAGS = -ludev -ldns_sd -lmonome -llo

SERIALOSC_OBJS  = serialosc.o
SERIALOSC_OBJS += monitor_linux.o

SERIALOSC_OBJS += osc.o
SERIALOSC_OBJS += osc_methods.o
SERIALOSC_OBJS += osc_sys_methods.o

all: serialosc

install:
	$(INSTALL) -d $(BINDIR)

	echo "  INSTALL src/serialosc -> $(BINDIR)/serialosc"
	$(INSTALL) serialosc $(BINDIR)/serialosc

clean:
	echo "  CLEAN   src"
	rm -f serialosc *.o

serialosc: $(SERIALOSC_OBJS)
	echo "  LD      src/serialosc"
	$(LD) $(LDFLAGS) -o $@ $(SERIALOSC_OBJS)

.c.o:
	echo "  CC      src/$@"
	$(CC) $(CFLAGS) -c $< -o $@
