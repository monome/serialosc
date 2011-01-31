CFLAGS += -I./private $(LO_CFLAGS) $(LIBMONOME_CFLAGS) $(LIBCONFUSE_CFLAGS)
LDFLAGS += $(LO_LDFLAGS) $(LIBMONOME_LDFLAGS) $(LIBCONFUSE_LDFLAGS)

SERIALOSC_OBJS  = serialosc.o
SERIALOSC_OBJS += util.o
SERIALOSC_OBJS += server.o
SERIALOSC_OBJS += config.o
SERIALOSC_OBJS += osc/util.o
SERIALOSC_OBJS += osc/methods.o
SERIALOSC_OBJS += osc/sys_methods.o

SERIALOSC_OBJS += detector/$(PLATFORM).o
SERIALOSC_OBJS += event_loop/$(EVLOOP).o

all: serialosc

install: all
	$(INSTALL) -d $(BINDIR)

	echo "  INSTALL src/serialosc -> $(BINDIR)/serialosc"
	$(INSTALL) serialosc $(BINDIR)/serialosc

clean:
	echo "  CLEAN   src"
	rm -f serialosc $(SERIALOSC_OBJS)

serialosc: $(SERIALOSC_OBJS)
	echo "  LD      src/serialosc"
	$(LD) $(LDFLAGS) -o $@ $(SERIALOSC_OBJS)

.c.o:
	echo "  CC      src/$@"
	$(CC) $(CFLAGS) -c $< -o $@
