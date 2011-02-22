CFLAGS += -I./private $(LO_CFLAGS) $(LIBMONOME_CFLAGS) $(LIBCONFUSE_CFLAGS)
LDFLAGS += $(LO_LDFLAGS) $(LIBMONOME_LDFLAGS) $(LIBCONFUSE_LDFLAGS)

PLATFORM_linux_OBJS   = platform/posix.o platform/linux.o
PLATFORM_darwin_OBJS  = platform/posix.o platform/darwin.o
PLATFORM_windows_OBJS = platform/windows.o

SERIALOSC_OBJS  = serialosc.o
SERIALOSC_OBJS += util.o
SERIALOSC_OBJS += server.o
SERIALOSC_OBJS += config.o
SERIALOSC_OBJS += osc/util.o
SERIALOSC_OBJS += osc/mext_methods.o
SERIALOSC_OBJS += osc/sys_methods.o

SERIALOSC_OBJS += $(PLATFORM_$(PLATFORM)_OBJS)
SERIALOSC_OBJS += detector/$(DETECTOR).o
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
	$(LD) -o $@ $(SERIALOSC_OBJS) $(LDFLAGS) 

.c.o:
	echo "  CC      src/$@"
	$(CC) $(CFLAGS) -c $< -o $@
