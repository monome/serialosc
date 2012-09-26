#!/usr/bin/env python

import time
import sys

top = "."
out = "build"

# change this stuff

APPNAME = "serialosc"
VERSION = "1.1"

#
# dep checking functions
#

def check_poll(conf):
	# borrowed from glib's poll test

	code = """
		#include <stdlib.h>
		#include <poll.h>

		int main(int argc, char **argv) {
		    struct pollfd fds[1];

		    fds[0].fd = open("/dev/null", 1);
		    fds[0].events = POLLIN;

		    if( poll(fds, 1, 0) < 0 || fds[0].revents & POLLNVAL )
		        exit(1);
		    exit(0);
		}"""

	conf.check_cc(
		define_name="HAVE_WORKING_POLL",
		mandatory=False,
		quote=0,

		execute=True,

		fragment=code,

		msg="Checking for working poll()",
		errmsg="no (will use select())")

def check_udev(conf):
	conf.check_cc(
		define_name="HAVE_LIBUDEV",
		mandatory=False,
		quote=0,

		execute=True,

		lib="udev",
		uselib_store="UDEV",

		msg="Checking for libudev",
		errmsg="no (will use sysfs)")

def check_liblo(conf):
	conf.check_cc(
		define_name="HAVE_LO",
		mandatory=True,
		quote=0,

		execute=True,

		lib="lo",
		uselib_store="LO",

		msg="Checking for liblo")

def check_confuse(conf):
	conf.check_cc(
		define_name="HAVE_CONFUSE",
		mandatory=True,
		quote=0,

		execute=True,

		lib="confuse",
		uselib_store="CONFUSE",

		msg="Checking for libconfuse")

def check_libmonome(conf):
	conf.check_cc(
		define_name="HAVE_LIBMONOME",
		mandatory=True,
		quote=0,

		execute=True,

		lib="monome",
		header_name="monome.h",
		uselib_store="LIBMONOME",

		msg="Checking for libmonome")


def check_dnssd_win(conf):
	conf.check_cc(
		mandatory=True,
		header_name="dns_sd.h",
		includes=["c:/program files/bonjour sdk/include"],
		uselib_store="DNSSD_INC")


def check_dnssd(conf):
	conf.check_cc(
		mandatory=True,
		header_name="dns_sd.h")

#
# waf stuff
#

def options(opt):
	opt.load("compiler_c")

	sosc_opts = opt.add_option_group("serialosc options")

	sosc_opts.add_option("--enable-multilib", action="store_true",
			default=False, help="on Darwin, build serialosc as a combination 32 and 64 bit executable [disabled by default]")

def configure(conf):
	# just for output prettifying
	# print() (as a function) ddoesn't work on python <2.7
	separator = lambda: sys.stdout.write("\n")

	separator()
	conf.load("compiler_c")
	conf.load("gnu_dirs")

	if conf.env.DEST_OS == "win32":
		conf.load("winres")
		conf.env.append_unique("LIBPATH", conf.env.LIBDIR)
		conf.env.append_unique("CFLAGS", conf.env.CPPPATH_ST % conf.env.INCLUDEDIR)

	#
	# conf checks
	#

	separator()

	if conf.env.DEST_OS != "win32":
		check_poll(conf)

	if conf.env.DEST_OS == "linux":
		check_udev(conf)

	check_libmonome(conf)
	check_liblo(conf)
	check_confuse(conf)

	if conf.env.DEST_OS == "win32":
		check_dnssd_win(conf)
	elif conf.env.DEST_OS != "darwin":
		check_dnssd(conf)
		conf.check_cc(lib='dl', uselib_store='DL', mandatory=True)

	separator()

	#
	# setting defines, etc
	#

	if conf.options.enable_multilib:
		conf.env.ARCH = ["i386", "x86_64"]

	if conf.env.DEST_OS == "win32":
		conf.define("WIN32", 1)
		conf.env.append_unique("LIB_LO", "ws2_32")
		conf.env.append_unique("LINKFLAGS", ["-Wl,--enable-stdcall-fixup"])
		conf.env.append_unique("WINRCFLAGS", ["-O", "coff"])
	elif conf.env.DEST_OS == "darwin":
		conf.env.append_unique("CFLAGS", ["-mmacosx-version-min=10.5"])
		conf.env.append_unique("LINKFLAGS", ["-mmacosx-version-min=10.5"])

	conf.env.VERSION = VERSION
	conf.env.append_unique("CFLAGS", ["-std=c99", "-Wall", "-Werror"])

def build(bld):
	bld.recurse("src")

def dist(dst):
	pats = [".git*", "**/.git*", ".travis.yml"]
	with open(".gitignore") as gitignore:
	    for l in gitignore.readlines():
	        if l[0] == "#":
	            continue

	        pats.append(l.rstrip("\n"))

	dst.excl = " ".join(pats)
