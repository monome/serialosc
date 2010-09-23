SHELL = /bin/sh

include config.mk

export CFLAGS  += -Wall -Werror -fPIC -DVERSION=\"$(VERSION)\" -DLIBSUFFIX=\".$(LIBSUFFIX)\" -DLIBDIR=\"$(LIBDIR)\"
export LDFLAGS += -L$(LIBDIR) -Wl,-rpath,$(LIBDIR)
export INSTALL = install

SUBDIRS = src

.SILENT:
.SUFFIXES:
.SUFFIXES: .c .o
.PHONY: all clean install config.mk

all:
	cd src; $(MAKE)

clean:
	cd src; $(MAKE) clean

distclean: clean
	rm -f config.mk

install:
	cd src; $(MAKE) install

config.mk:
	if [ ! -f config.mk ]; then \
	echo ;\
	echo "  _                " ;\
	echo " | |__   ___ _   _ " ;\
	echo " |  _ \ / _ \ | | |    you need to run " ;\
	echo " | | | |  __/ |_| |  ./configure first!" ;\
	echo " |_| |_|\___|\__, |" ;\
	echo "             |___/ " ;\
	echo ;\
	fi
