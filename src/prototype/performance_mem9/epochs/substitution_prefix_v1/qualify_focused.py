#!/usr/bin/env python3
"""Qualify one scan deletion against its exact parent with independent outputs."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

owner = Path(__file__).resolve().parent
trial = owner / 'private/substitution_prefix_v1_20261003'
parent = Path('/tmp/ap-performance-mem5-materialized-fields-current-e12-20261003')
tests = Path('/tmp/ap-performance-mem2-iadt-spine-current-e12-20261003/tests')
controls = owner / 'build.mk'
output = trial / 'focused'
output.mkdir()
temporary = trial / 'temporary'
temporary.mkdir()
records = []


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_source(root):
	for line in (root / 'source.sha256').read_text().splitlines():
		expected, name = line.split('  ', 1)
		assert sha(root / name) == expected, name


def run(label, argv, sanitizer=False):
	log = output / (label + '.log')
	environment = dict(os.environ, TMPDIR=str(temporary))
	if sanitizer:
		environment['ASAN_OPTIONS'] = 'detect_leaks=1:detect_stack_use_after_return=1:abort_on_error=1'
		environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
	with log.open('wb') as stream:
		process = subprocess.run(argv, env=environment, stdout=stream,
			stderr=subprocess.STDOUT, timeout=180)
	records.append({'label': label, 'argv': argv, 'exit': process.returncode,
		'sanitizer': sanitizer, 'log_sha256': sha(log)})
	(trial / 'focused-live.json').write_text(json.dumps(records, indent=2) + '\n')
	assert process.returncode == 0, log
	return log.read_text()


pins = {str(path): sha(path) for path in (controls, owner / 'substitution_prefix_test.c',
	Path(__file__), tests / 'core.c', tests / 'eval_io.c', tests / 'eval_io.sh', parent / 'src/Makefile')}
verify_source(parent)
verify_source(trial)
assert sha(parent / 'source.sha256') == '2344ad41d6db32ceddb5dbc9419aa8da97fcaa00253e075f5ff96b8f72f93b13'
assert sha(trial / 'source.sha256') == 'a1d03b837422bc5525cbbbd52b023533fa1efa3d3a5179fa898379d8643645bf'
binaries = {}
outcomes = {}
for flavor, flags in [('o2', '-std=c11 -Wall -Wextra -Werror -O2'),
	('san', '-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie')]:
	for variant, source in [('parent', parent), ('candidate', trial)]:
		build = trial / ('build-' + variant + '-' + flavor)
		names = ['substitution_prefix_test']
		if variant == 'candidate':
			names += ['core_test', 'eval_io_test']
		run('build-' + variant + '-' + flavor, ['make', '-j1', '-f', str(controls),
			'OVERLAY=' + str(source), 'ROOT=' + str(source / 'src') + '/',
			'TESTS=' + str(tests), 'BUILD=' + str(build), 'CFLAGS=' + flags]
			+ [str(build / name) for name in names])
		text = run('local-' + variant + '-' + flavor, [str(build / 'substitution_prefix_test')], flavor == 'san')
		rows = re.findall(r'Prefix case=(\d+) steps=(\d+) initial_reads=(\d+) status=(\d+) pass', text)
		assert len(rows) == 6 and [int(row[0]) for row in rows] == list(range(6)), text
		outcomes[(variant, flavor)] = rows
		binaries[(variant, flavor)] = build / 'substitution_prefix_test'
		if variant == 'candidate':
			for name in ('core_test', 'eval_io_test'):
				run(name + '-' + flavor, [str(build / name)], flavor == 'san')
			run('eval-io-full-' + flavor,
				['bash', str(tests / 'eval_io.sh'), str(build / 'eval_io_test')], flavor == 'san')
		print(json.dumps({'variant': variant, 'flavor': flavor, 'steps': [int(row[1]) for row in rows],
			'initial_reads': [int(row[2]) for row in rows]}), flush=True)
totals = [int(row[1]) for row in outcomes[('parent', 'o2')]]
for key, rows in outcomes.items():
	assert [int(row[1]) for row in rows] == totals, (key, rows)
for flavor in ('o2', 'san'):
	for variant in ('parent', 'candidate'):
		for case, total in enumerate(totals):
			for cut in range(total + 1):
				label = flavor + '-' + variant + '-' + str(case) + '-' + str(cut)
				image = output / (label + '.image')
				run('write-' + label, [str(binaries[(variant, flavor)]), 'write', str(image),
					str(case), str(cut)], flavor == 'san')
				for reader_flavor in ('o2', 'san'):
					for reader in ('parent', 'candidate'):
						text = run('read-' + reader + '-' + reader_flavor + '-' + label,
							[str(binaries[(reader, reader_flavor)]), 'read', str(image)], reader_flavor == 'san')
						assert f'case={case} total={total} pass' in text
		print(json.dumps({'variant': variant, 'flavor': flavor,
			'writes': sum(total + 1 for total in totals), 'fresh_reads_per_write': 4}), flush=True)
for path, expected in pins.items():
	assert sha(Path(path)) == expected, path
verify_source(parent)
verify_source(trial)
(trial / 'focused-summary.json').write_text(json.dumps({'terminal': True,
	'records': len(records), 'failed': 0, 'totals': totals,
	'cuts_per_variant_build': sum(total + 1 for total in totals),
	'parent_source128': sha(parent / 'source.sha256'), 'candidate_source128': sha(trial / 'source.sha256'),
	'only_runtime_change': ['src/eval.c'], 'pins': pins,
	'initial_projection_reads': {variant + '-' + flavor: [int(row[2]) for row in rows]
		for (variant, flavor), rows in outcomes.items()},
	'new_owner_codec_layout_allocator_authority': False, 'all_probe_children_stopped': True,
	'actual_wall_RSS_or_speed_gain': False, 'broader_affected_controls_pending': True}, indent=2) + '\n')
print(json.dumps({'terminal': True, 'records': len(records), 'failed': 0,
	'summary_sha256': sha(trial / 'focused-summary.json')}), flush=True)
