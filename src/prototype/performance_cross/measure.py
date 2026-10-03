#!/usr/bin/env python3
"""Run pinned fresh-process samples during an explicitly granted exclusive slot."""

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument('configuration', type=Path)
	parser.add_argument('output', type=Path)
	parser.add_argument('--granted-slot', required=True,
		help='Exact coordinator grant identifier; provide only after the grant arrives')
	args = parser.parse_args()
	args.configuration = args.configuration.resolve()
	args.output = args.output.resolve()
	configuration = json.loads(args.configuration.read_text())
	assert configuration['exclusive_slot_required']
	assert 0 < configuration['maximum_seconds'] <= 2400
	assert 0 < configuration['per_run_seconds'] <= 180
	for name, expected in configuration['input_sha256'].items():
		assert sha(Path(name)) == expected, name
	args.output.mkdir(parents=True, exist_ok=False)
	deadline = time.monotonic() + configuration['maximum_seconds']
	report = {'complete': False, 'granted_slot': args.granted_slot,
		'configuration_sha256': sha(args.configuration),
		'conditions': 'Sequential fresh processes; warm filesystem after correctness qualification; no cache flush.',
		'RSS_metric': 'GNU time %M in KiB for the wrapped command; not a sum of concurrently resident processes.',
		'wall_metric': 'Launcher perf_counter_ns plus GNU time %e; includes process and wrapper startup.',
		'configuration': configuration, 'samples': []}
	destination = args.output / 'measurements.json'
	destination.write_text(json.dumps(report, indent=2) + '\n')
	for index, job in enumerate(configuration['jobs']):
		remaining = deadline - time.monotonic()
		if remaining <= 1:
			report['deadline_reached'] = True
			break
		limit = min(configuration['per_run_seconds'], remaining)
		folder = args.output / ('sample-' + str(index))
		folder.mkdir()
		command = job['argv'][:]
		if job.get('fresh_source'):
			original = Path(job['fresh_source'])
			copied = folder / original.name
			shutil.copy2(original, copied)
			command = [str(copied.resolve()) if value == str(original) else value for value in command]
		environment = os.environ.copy()
		environment.update(job.get('environment_overrides', {}))
		metrics = folder / 'time.tsv'
		wrapped = ['/usr/bin/time', '-o', str(metrics), '-f', '%e\t%U\t%S\t%M\t%x',
			'timeout', '--kill-after=1s', str(limit) + 's', *command]
		log = folder / 'output.log'
		started = time.perf_counter_ns()
		with log.open('wb') as stream:
			result = subprocess.run(wrapped, cwd=folder, env=environment,
				stdout=stream, stderr=subprocess.STDOUT)
		elapsed = (time.perf_counter_ns() - started) / 1000000000
		metric_lines = [line.split('\t') for line in metrics.read_text().splitlines()
			if len(line.split('\t')) == 5]
		assert len(metric_lines) == 1
		wall, user, system, rss, measured_exit = metric_lines[0]
		output = log.read_text(errors='replace')
		passed = result.returncode == 0
		if job['system'] == 'ap':
			passed = passed and output.startswith('done steps=')
		row = {'index': index, 'case': job['case'], 'system': job['system'],
			'variant': job.get('variant'), 'repetition': job['repetition'],
			'command': command, 'wrapped_command': wrapped,
			'environment_overrides': job.get('environment_overrides', {}),
			'exit': result.returncode, 'measured_exit': int(measured_exit), 'passed': passed,
			'timeout': result.returncode in (124, 137),
			'wall_seconds_launcher': elapsed, 'wall_seconds_GNU_time': float(wall),
			'user_seconds': float(user), 'system_seconds': float(system),
			'maximum_RSS_KiB': int(rss), 'log_sha256': sha(log), 'metrics_sha256': sha(metrics)}
		if job['system'] == 'ap' and passed:
			row['charged_steps'] = int(output.split('steps=', 1)[1].split()[0])
		report['samples'].append(row)
		destination.write_text(json.dumps(report, indent=2) + '\n')
		print(json.dumps({key: row[key] for key in ('case', 'system', 'variant', 'repetition', 'exit', 'passed')}), flush=True)
	for name, expected in configuration['input_sha256'].items():
		assert sha(Path(name)) == expected, name
	report.update({'complete': True, 'planned_samples': len(configuration['jobs']),
		'completed_samples': len(report['samples']),
		'all_planned_samples_passed': len(report['samples']) == len(configuration['jobs'])
			and all(row['passed'] for row in report['samples']), 'input_hashes_before_after': True})
	destination.write_text(json.dumps(report, indent=2) + '\n')
	with (args.output / 'measurements.tsv').open('w') as stream:
		fields = ['case', 'system', 'variant', 'repetition', 'exit', 'passed',
			'wall_seconds_launcher', 'wall_seconds_GNU_time', 'user_seconds', 'system_seconds',
			'maximum_RSS_KiB', 'charged_steps']
		writer = csv.DictWriter(stream, fieldnames=fields, delimiter='\t', extrasaction='ignore')
		writer.writeheader()
		writer.writerows(report['samples'])
	if not report['all_planned_samples_passed']:
		raise SystemExit(1)


if __name__ == '__main__':
	main()
