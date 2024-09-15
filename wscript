#!/usr/bin/env python

import subprocess
import time
import sys

top = "."
out = "build"

# change this stuff

APPNAME = "serialosc"
VERSION = "1.4.4"

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

		    if (poll(fds, 1, 0) < 0 || fds[0].revents & POLLNVAL)
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
		mandatory=True,
		quote=0,

		lib="udev",
		uselib_store="UDEV",

		msg="Checking for libudev")

def check_liblo(conf):
	conf.check_cc(
		define_name="HAVE_LO",
		mandatory=True,
		quote=0,

		lib="lo",
		uselib_store="LO",

		msg="Checking for liblo")

def check_libmonome(conf):
	conf.check_cc(
		define_name="HAVE_LIBMONOME",
		mandatory=True,
		quote=0,

		lib="monome",
		header_name="monome.h",
		uselib_store="LIBMONOME",

		msg="Checking for libmonome")

def check_libuv(conf):
	conf.check_cc(
		define_name="HAVE_LIBUV",
		mandatory=True,
		quote=0,

		lib="uv",
		uselib_store="LIBUV",

		msg="Checking for libuv")

def check_strfuncs(ctx):
	check_strfunc_template = """
		#include <string.h>

		int main(int argc, char **argv) {{
			void (*p)();
			(void)argc; (void)argv;
			p=(void(*)())({});
			return !p;
		}}
	"""
	check = lambda func_name: ctx.check_cc(
			msg='Checking for {}'.format(func_name),
			define_name='HAVE_{}'.format(func_name.upper()),
			mandatory=False,
			execute=True,
			fragment=check_strfunc_template.format(func_name))

	check('strdup')
	check('strndup')
	check('strcasecmp')

def check_miscfuncs(ctx):
	check_strfunc_template = """
		#include <stdio.h>
		#include <stdlib.h>

		int main(int argc, char **argv) {{
			void (*p)();
			(void)argc; (void)argv;
			p=(void(*)())({});
			return !p;
		}}
	"""
	check = lambda func_name: ctx.check_cc(
			msg='Checking for {}'.format(func_name),
			define_name='HAVE_{}'.format(func_name.upper()),
			mandatory=False,
			execute=True,
			fragment=check_strfunc_template.format(func_name))

	check('fmemopen')
	check('funopen')
	check('reallocarray')

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

def check_submodules(conf):
	if not conf.path.find_resource('third-party/optparse/optparse.h'):
		raise conf.errors.ConfigurationError(
			"Submodules aren't initialized!\n"
			"Make sure you've done `git submodule init && git submodule update`.")

def load_tools(ctx):
	tooldir = ctx.path.find_dir('waftools').abspath()
	load_tool = lambda t: ctx.load(t, tooldir=tooldir)

	load_tool('winres_gen')

def override_find_program(prefix):
	from waflib.Configure import find_program as orig_find
	from waflib.Configure import conf

	if prefix[-1] != '-':
		prefix += '-'

	@conf
	def find_program(self, filename, **kw):
		if type(filename) == str:
			return orig_find(self, prefix + filename, **kw)
		else:
			return orig_find(self, [prefix + x for x in filename], **kw)
		return orig_find(self, filename, **kw)

#
# waf stuff
#

def options(opt):
	opt.load("compiler_c")

	xcomp_opts = opt.add_option_group('cross-compilation')
	xcomp_opts.add_option('--host', action='store', default=False)

	sosc_opts = opt.add_option_group("serialosc options")
	sosc_opts.add_option("--enable-multilib", action="store_true",
			default=False, help="on Darwin, build serialosc as a combination 32 and 64 bit executable [disabled by default]")
	sosc_opts.add_option("--disable-zeroconf", action="store_true",
			default=False, help="disable all zeroconf code, including runtime loading of the DNSSD library.")
	sosc_opts.add_option('--enable-debug', action='store_true',
			default=False, help="Build debuggable binaries")

def configure(conf):
	# just for output prettifying
	# print() (as a function) ddoesn't work on python <2.7
	separator = lambda: sys.stdout.write("\n")

	if conf.options.host:
		override_find_program(conf.options.host)

	separator()
	conf.load('compiler_c')
	conf.load('gnu_dirs')
	load_tools(conf)

	if conf.env.DEST_OS == "win32":
		conf.load("winres")

		conf.env.append_unique("LIBPATH", conf.env.LIBDIR)
		conf.env.append_unique("CFLAGS", conf.env.CPPPATH_ST % conf.env.INCLUDEDIR)
		conf.env.append_unique("LIB", ['setupapi'])

	if conf.options.host:
		conf.env.append_unique("LIBPATH", conf.env.PREFIX + '/lib')
		conf.env.append_unique("CFLAGS",
				conf.env.CPPPATH_ST % conf.env.PREFIX + '/include')

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

	check_libuv(conf)
	check_submodules(conf)

	# stuff for libconfuse
	check_strfuncs(conf)
	check_miscfuncs(conf)

	conf.check_cc(mandatory=False, define_name='HAVE_UNISTD_H', header_name='unistd.h')
	conf.check_cc(mandatory=False, define_name='HAVE_STRING_H', header_name='string.h')
	conf.check_cc(mandatory=False, define_name='HAVE_STRINGS_H', header_name='strings.h')
	conf.check_cc(mandatory=False, define_name='HAVE_SYS_STAT_H', header_name='sys/stat.h')
	conf.check_cc(mandatory=False, define_name='HAVE_WINDOWS_H', header_name='windows.h')

	if conf.env.DEST_OS == "win32":
		if not conf.options.disable_zeroconf:
			check_dnssd_win(conf)
	elif conf.env.DEST_OS != "darwin":
		if not conf.options.disable_zeroconf:
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
		conf.env.append_unique("LINKFLAGS",
				['-static', '-static-libgcc',
					'-Wl,--enable-stdcall-fixup'])
		conf.env.append_unique("WINRCFLAGS", ["-O", "coff"])
	elif conf.env.DEST_OS == "darwin":
		conf.env.append_unique("CFLAGS", ["-mmacosx-version-min=10.13"])
		conf.env.append_unique("LINKFLAGS", ["-mmacosx-version-min=10.13"])

	if conf.options.disable_zeroconf:
		conf.define("SOSC_NO_ZEROCONF", True)
		conf.env.SOSC_NO_ZEROCONF = True


	if conf.options.enable_debug:
		conf.env.append_unique("CFLAGS", ["-std=c17",  "-Wall", "-g", "-Og"])
	else:
		conf.env.append_unique("CFLAGS", ["-std=c17", "-Wall", "-Werror", "-O2"])


	if conf.env.CC_NAME in ["gcc", "clang"]:
		# FIXME: a poor solution perhaps, it will do for now.
		# would be better to fix all the relevant signatures.
		conf.env.append_unique("CFLAGS", ["-Wno-incompatible-pointer-types"])


	conf.env.VERSION = VERSION

	try:
		import os

		devnull = open(os.devnull, 'w')

		conf.env.GIT_COMMIT = subprocess.check_output(
			["git", "rev-parse", "--verify", "--short", "HEAD"],
			stderr=devnull).decode().strip()
	except subprocess.CalledProcessError:
		conf.env.GIT_COMMIT = ''

	conf.define("VERSION", VERSION)
	conf.define("_GNU_SOURCE", 1)
	conf.define("GIT_COMMIT", conf.env.GIT_COMMIT)

def build(bld):
	bld.recurse("third-party")
	bld.recurse("src")

def dist(dst):
	pats = [".git*", "**/.git*", ".travis.yml", "**/__pycache__"]
	with open(".gitignore") as gitignore:
	    for l in gitignore.readlines():
	        if l[0] == "#":
	            continue

	        pats.append(l.rstrip("\n"))

	dst.excl = " ".join(pats)
