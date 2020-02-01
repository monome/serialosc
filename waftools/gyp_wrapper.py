#!/usr/bin/env python

import os

from waflib.Configure import conf

def parse_gyp_file(path):
	parsed = None
	with open(path) as f:
		# just going to assume the gyp file is well-formed
		parsed = eval(f.read())

	return parsed

def find_target(tgts, name):
	r = [t for t in tgts if t['target_name'] == name]

	if len(r) == 1:
		return r[0]
	elif not r:
		return None
	else:
		return r

def unpack_defs(into, defs):
	for d in defs:
		if d not in into:
			into[d] = []

		if isinstance(defs[d], list):
			into[d].extend(defs[d])
		elif isinstance(defs[d], dict):
			unpack_defs(into, defs[d])

def eval_condition_list(clist, **kwargs):
	ret = {}

	for c in clist:
		cond = None

		try:
			cond = eval(c[0], kwargs)
		except NameError:
			cond = False

		if cond is True:
			defs = c[1]
		else:
			try:
				defs = c[2]
			except IndexError:
				continue

		unpack_defs(ret, defs)

	return ret

def eval_condition_list_recursively(clist, **kwargs):
	ret = {}

	while True:
		defs = eval_condition_list(clist, **kwargs)

		for d in defs:
			if d not in ret:
				ret[d] = []

			ret[d].extend(defs[d])

		if 'conditions' in defs:
			clist = defs['conditions']
		else:
			break

	return ret

@conf
def target_from_gyp(ctx, path, target):
	n = ctx.path.get_src().find_resource(path)
	gyp = parse_gyp_file(n.abspath())

	t = find_target(gyp['targets'], target)

	from_gyp = {
		'sources':   t['sources'],
		'defines':   [],
		'cflags':	[],
		'ldflags':   [],
		'libraries': []}

	os_map = {
		'linux':  'linux',
		'win32':  'win',
		'darwin': 'mac'
	}

	for clist in (gyp['variables']['conditions'], t['conditions'],
			t['direct_dependent_settings']['conditions']):
		r = eval_condition_list_recursively(clist,
				OS=os_map[ctx.env.DEST_OS],
				library='static_library')

		[from_gyp[x].extend(r.get(x, [])) for x in from_gyp.keys()]

	containing_dir = os.path.split(path)[0]
	from_gyp['sources'] = [
			'{}/{}'.format(containing_dir, s) for s in from_gyp['sources']
			if os.path.splitext(s)[1] in ('.c', '.cc', '.cpp')]

	# libraries from gyp all start with -l
	from_gyp['libraries'] = [l[2:] for l in from_gyp['libraries']]

	return from_gyp

# vim: set ts=4 sts=4 noet :
