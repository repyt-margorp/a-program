#!/usr/bin/env python3
"""Check typed callers and unchanged public images without another full suite."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

owner = Path(__file__).resolve().parent
worker = owner.parents[2]
trial = owner / 'private/substitution_prefix_v1_20261003'
parent = Path('/tmp/ap-performance-mem5-materialized-fields-current-e12-20261003')
fixtures = Path('/tmp/ap-performance-mem2-iadt-spine-current-e12-20261003')
reference = Path('/tmp/ap-performance-mem3-fold-cost-request-20261003/configuration-proposed.json')
output = trial / 'affected'
output.mkdir(exist_ok=False)
records = []


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_source(root):
	for line in (root / 'source.sha256').read_text().splitlines():
		expected, name = line.split('  ', 1)
		assert sha(root / name) == expected, name


def run(label, argv, sanitizer=False, expected=0):
	log = output / (label + '.log')
	environment = dict(os.environ, TMPDIR=str(trial / 'temporary'))
	if sanitizer:
		environment['ASAN_OPTIONS'] = 'detect_leaks=1:detect_stack_use_after_return=1:abort_on_error=1'
		environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
	with log.open('wb') as stream:
		process = subprocess.run(argv, env=environment, stdout=stream,
			stderr=subprocess.STDOUT, timeout=180)
	records.append({'label': label, 'argv': argv, 'exit': process.returncode,
		'expected_exit': expected, 'sanitizer': sanitizer, 'log_sha256': sha(log)})
	(trial / 'affected-live.json').write_text(json.dumps(records, indent=2) + '\n')
	assert process.returncode == expected, log
	return log.read_text()


jobs = [job for job in json.loads(reference.read_text())['jobs']
	if job['case'] in ('list', 'trees4', 'imported-local-sorted')
	and job['variant'] == 'fold-empty-tail' and job['repetition'] == 1]
pins = {str(path): sha(path) for path in (Path(__file__), reference,
	fixtures / 'tests/iadt.c', fixtures / 'checkpoint_tests/normalization_checkpoint_test.c',
	fixtures / 'examples/09_list_induction.p', worker / 'src/prototype/image_audit/partition_fuel.sh')}
for job in jobs:
	for argument in job['argv'][1:]:
		path = Path(argument)
		if path.is_file():
			pins[str(path)] = sha(path)
verify_source(parent)
verify_source(trial)
for flavor, flags in [('o2', '-std=c11 -Wall -Wextra -Werror -O2'),
	('san', '-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie')]:
	build = trial / ('build-candidate-' + flavor)
	targets = ['iadt_test', 'artifact_normalization_checkpoint_test']
	if flavor == 'o2': targets += ['pointer-check']
	run('build-' + flavor, ['make', '-j1', '-f', str(owner / 'build.mk'),
		'OVERLAY=' + str(trial), 'ROOT=' + str(trial / 'src') + '/', 'BUILD=' + str(build),
		'REPO=' + str(fixtures), 'TESTS=' + str(fixtures / 'tests'),
		'CHECKPOINT_TESTS=' + str(fixtures / 'checkpoint_tests') + '/', 'CFLAGS=' + flags]
		+ [str(build / name) for name in targets])
	for name in ('iadt_test', 'artifact_normalization_checkpoint_test'):
		run(name + '-' + flavor, [str(build / name)], flavor == 'san')
	print(json.dumps({'flavor': flavor, 'two_affected_units': 'pass'}), flush=True)
for job in jobs:
	text = run('cli-' + job['case'], [str(trial / 'build-candidate-o2/pointer-check'), *job['argv'][1:]])
	assert text.strip() == f"done steps={job['expected_charged_steps']}", text
partitions = trial / 'partitions'
run('public-partitions', ['bash', str(worker / 'src/prototype/image_audit/partition_fuel.sh'),
	str(trial / 'build-candidate-o2/pointer-check'), str(fixtures / 'examples/09_list_induction.p'),
	str(partitions), 'ordinary'], expected=1)
assert (partitions / 'partitions.tsv').read_bytes() == (parent / 'partitions/partitions.tsv').read_bytes()
images = sorted((parent / 'partitions').glob('*.a'))
assert len(images) == 52
for path in images:
	assert sha(path) == sha(partitions / path.name), path.name
for path, expected in pins.items():
	assert sha(Path(path)) == expected, path
verify_source(parent)
verify_source(trial)
assert len(records) == 10
(trial / 'affected-summary.json').write_text(json.dumps({'terminal': True,
	'records': records, 'new_failures': 0, 'original_strict_recipe_failures': 1,
	'original_strict_reload_failures': 3, 'public52_images_equal': True,
	'full_verdict_fuel_TSV_equal': True, 'pins': pins,
	'completed_charged_steps': {job['case']: job['expected_charged_steps'] for job in jobs},
	'fresh_full_acceptance_or_other_six_checkpoints_claim': False,
	'actual_speed_wall_RSS_gain': False, 'all_probe_children_stopped': True}, indent=2) + '\n')
print(json.dumps({'terminal': True, 'records': len(records), 'new_failures': 0,
	'original_strict_recipe_failures': 1, 'summary_sha256': sha(trial / 'affected-summary.json')}), flush=True)
