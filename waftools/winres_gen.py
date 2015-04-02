#!/usr/bin/env python

from waflib.TaskGen import feature, before_method
import os
import re

non_decimal = re.compile(r"[^\d.]+")

def strip_non_numeric(s):
	return non_decimal.sub("", s)

def win_version_string(version):
	"""FILEVERSION and PRODUCTVERSION in .rc files look like:

		FILEVERSION    1,0,0,0
		PRODUCTVERSION 1,0,0,0

	this function turns a normal version string (like "1.1") into
	a windowsy one."""

	v = strip_non_numeric(version).split(".")
	pad = ["0" for x in range(4 - len(v))]

	v.extend(pad)
	return ",".join(v[:4])

@feature('winres_gen')
@before_method('apply_link')
def winres_gen(self):
	executable = os.path.basename(self.target)

	src_path = 'src/common/platform/winres/serialosc.rc.in'
	tgt_path = '{}.rc'.format(os.path.splitext(executable)[0])

	src = self.bld.srcnode.find_node(src_path)
	tgt = self.path.get_bld().find_or_declare(tgt_path)

	subst_task = self.create_task('subst', src, tgt)
	subst_task.install_path = None

	subst_task.generator.dct = {
		'EXECUTABLE': executable,
		'GIT_COMMIT': self.env.GIT_COMMIT,
		'WIN_VERSION': win_version_string(self.env.VERSION),
		'VERSION': self.env.VERSION
	}

# vim: set ts=4 sts=4 noet :
