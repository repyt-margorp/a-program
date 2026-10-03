#!/usr/bin/env python3
"""Compare completed state and ordinary images without collecting timings."""
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_manifest(root, path):
	for line in path.read_text().splitlines():
		expected, relative = line.split('  ', 1)
		assert sha(root / relative) == expected, relative


def run(label, argv, out, records, allowed=(0,)):
	log = out / (label + '.log')
	with log.open('wb') as stream:
		result = subprocess.run(argv, stdout=stream, stderr=subprocess.STDOUT, timeout=180)
	record = {'label': label, 'argv': [str(x) for x in argv], 'exit': result.returncode,
		'log': str(log), 'log_sha256': sha(log)}
	records.append(record)
	(out / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
	print(json.dumps({'label': label, 'exit': result.returncode}), flush=True)
	assert result.returncode in allowed, record
	return log.read_text()


def main():
	config_path, out = map(Path, sys.argv[1:])
	out.mkdir(exist_ok=False)
	config = json.loads(config_path.read_text())
	assert config['job_epoch'] == 'b2d66682be75c21a3c2a8c5138db9d3f0c5e0eb2'
	assert config['performance_commit'] == '05390513bca302b4a219881994c5bbd69d5b733a'
	for path, expected in config['input_sha256'].items():
		assert sha(Path(path)) == expected, path
	jobs = [row for row in config['jobs'] if row['system'] == 'ap' and row['repetition'] == 1]
	assert len(jobs) == 6
	roots = {
		'baseline': Path('/tmp/ap-performance-measurement-baseline-e6-20261003'),
		'head': Path('/tmp/ap-performance-current-joint-898463e-b2d6668-20261003'),
	}
	for root in roots.values():
		verify_manifest(root, root / 'verification.sha256')
	build_root = Path('/tmp/ap-performance-e6-state-images-20261003')
	provider = build_root / 'quick-local-provider.p'
	assert sha(provider) == 'c22eafc8fd288a29abce7b5ececf654c4aa826281e0dc4194eea453e464c2e77'
	records, census, images = [], {}, {}
	for job in jobs:
		case, variant = job['case'], job['variant']
		argv = list(job['argv'])
		assert argv[0] == str(roots[variant] / 'build/pointer-check')
		if '--imports' in argv:
			argv[argv.index('--imports') + 1] = str(provider)
		name = variant + '-' + case
		budget = '1000000000' if case == 'trees400' else '10000000'
		text = run(name + '-census', [str(build_root / (variant + '-build/import_state_audit')),
			argv[-1], str(provider) if '--imports' in argv else '-', '0', '100', '1000', budget], out, records)
		rows = [{key: int(value) for key, value in row.items()}
			for row in csv.DictReader(io.StringIO(text), delimiter='\t')]
		assert len(rows) == 4 and rows[-1]['status'] == 1
		assert rows[-1]['steps'] == {'list': {'baseline': 1921, 'head': 1915},
			'imported-local-sorted': {'baseline': 766477, 'head': 761848},
			'trees400': {'baseline': 227277774, 'head': 183507626}}[case][variant]
		census[name] = rows
		image = out / (name + '.a')
		text = run(name + '-save', [argv[0], '--save', str(image), *argv[1:]], out, records)
		assert text.strip() == 'done steps=' + str(rows[-1]['steps']), text
		images[name] = {'path': str(image), 'bytes': image.stat().st_size, 'sha256': sha(image),
			'source_steps': rows[-1]['steps']}
		for reader, root in roots.items():
			load = [str(root / 'build/pointer-check'), '--legacy-intrinsic-dot', '--load']
			text = run(name + '-read-' + reader, [*load, '--steps', '1000000000', str(image)], out, records)
			assert text.startswith('done steps='), text
			images[name][reader + '_ordinary_readback'] = text.strip()
			resave = out / (name + '-zero-resave-' + reader + '.a')
			text = run(name + '-zero-' + reader, [*load, '--steps', '0', '--save', str(resave), str(image)],
				out, records, allowed=(0, 3))
			images[name][reader + '_zero_step'] = {'exit': records[-1]['exit'], 'output': text.strip(),
				'resave_sha256': sha(resave), 'byte_equal': image.read_bytes() == resave.read_bytes()}
			assert images[name][reader + '_zero_step']['byte_equal']
	comparison = {}
	for case in ('list', 'imported-local-sorted', 'trees400'):
		left, right = census['baseline-' + case][-1], census['head-' + case][-1]
		comparison[case] = {
			'count_deltas_head_minus_baseline': {key: right[key] - left[key] for key in left},
			'completed_image_byte_equal': Path(images['baseline-' + case]['path']).read_bytes()
				== Path(images['head-' + case]['path']).read_bytes(),
			'completed_image_bytes_baseline': images['baseline-' + case]['bytes'],
			'completed_image_bytes_head': images['head-' + case]['bytes'],
		}
	for root in roots.values():
		verify_manifest(root, root / 'verification.sha256')
	for path, expected in config['input_sha256'].items():
		assert sha(Path(path)) == expected, path
	summary = {'terminal': True, 'lane': 'performance', 'epoch': 'E6 retained-state and image follow-up',
		'configuration': str(config_path), 'configuration_sha256': sha(config_path),
		'canonical_producer': config['canonical_producer'], 'job_epoch': config['job_epoch'],
		'performance_commit': config['performance_commit'], 'census': census, 'images': images,
		'comparisons': comparison, 'commands': len(records), 'census_and_source_save_commands_pass': True,
		'all_full_ordinary_fresh_readbacks_pass': True, 'all_zero_step_resaves_byte_equal': True,
		'zero_step_exit3_is_pending_and_is_not_an_acceptance_pass': True,
		'census_measures_retained_counts_and_computed_job_bytes_not_live_memory': True,
		'comparative_wall_RSS_collected': False, 'owner_runtime_or_frozen_file_edits': [],
		'all_source_input_and_frozen_evidence_hashes_before_after_match': True}
	(out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
	print(json.dumps({'terminal': True, 'summary_sha256': sha(out / 'summary.json'), 'comparisons': comparison}), flush=True)


if __name__ == '__main__':
	main()
