CFLAGS += -I./private $(LO_CFLAGS) $(LIBMONOME_CFLAGS) $(LIBCONFUSE_CFLAGS)
LDFLAGS += $(LO_LDFLAGS) $(LIBMONOME_LDFLAGS) $(LIBCONFUSE_LDFLAGS)

POSIX_OBJS  = manager/posix.o
POSIX_OBJS += platform/posix.o

PLATFORM_linux_OBJS   = $(POSIX_OBJS) platform/linux.o
PLATFORM_darwin_OBJS  = $(POSIX_OBJS) platform/darwin.o
PLATFORM_windows_OBJS = manager/windows.o platform/windows.o

SERIALOSC_OBJS  = serialosc.o
SERIALOSC_OBJS += ipc.o
SERIALOSC_OBJS += util.o
SERIALOSC_OBJS += server.o
SERIALOSC_OBJS += config.o
SERIALOSC_OBJS += osc/util.o
SERIALOSC_OBJS += osc/mext_methods.o
SERIALOSC_OBJS += osc/sys_methods.o

SERIALOSC_OBJS += $(PLATFORM_$(PLATFORM)_OBJS)
SERIALOSC_OBJS += detector/$(DETECTOR).o
SERIALOSC_OBJS += event_loop/$(EVLOOP).o

all: serialoscd

install: all
	$(INSTALL) -d $(BINDIR)

	echo "  INSTALL src/serialosc -> $(BINDIR)/serialosc"
	$(INSTALL) serialosc $(BINDIR)/serialosc

clean:
	echo "  CLEAN   src"
	rm -f serialoscd $(SERIALOSC_OBJS)

serialoscd: $(SERIALOSC_OBJS)
	echo "  LD      src/serialoscd"
	$(LD) -o $@ $(SERIALOSC_OBJS) $(LDFLAGS) 

.c.o:
	echo "  CC      src/$@"
	$(CC) $(CFLAGS) -c $< -o $@
