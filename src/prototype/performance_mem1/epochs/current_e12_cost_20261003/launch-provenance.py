#!/usr/bin/env python3
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import subprocess

request = Path('/tmp/ap-performance-mem1-v3-current-e12-cost-request-20261003')
original = request / 'configuration-final-proposed.json'
grant_path = Path('/tmp/a-program-merge-mem1-v3-e12-exclusive-grant-20261003.json')
grant = json.loads(grant_path.read_text())
runner = Path('/tmp/ap-performance-measure-gnu-time-e6-20261003.py')


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


assert grant['id'] == 'MEM1V3-E12-20261003T154600Z-600'
assert sha(original) == grant['original_config_sha256']
config = json.loads(original.read_text())
assert len(config['jobs']) == 36 and len(config['input_sha256']) == 280
for name, expected in config['input_sha256'].items():
	assert sha(Path(name)) == expected, name
now = datetime.now(timezone.utc)
not_before = datetime.fromisoformat(grant['not_before_UTC'].replace('Z', '+00:00'))
deadline = datetime.fromisoformat(grant['hard_deadline_UTC'].replace('Z', '+00:00'))
assert not_before <= now < deadline
remaining = (deadline - now).total_seconds()
derived = dict(config)
derived['maximum_seconds'] = min(600, math.floor(remaining) - 2)
assert derived['maximum_seconds'] > 0
assert {key for key in config if config[key] != derived[key]} <= {'maximum_seconds'}
path = request / 'configuration-granted-derived.json'
path.write_text(json.dumps(derived, indent=2) + '\n')
out = Path('/tmp/ap-performance-mem1-v3-e12-exclusive-samples-20261003')
argv = ['python3', str(runner), str(path), str(out), '--granted-slot', grant['id']]
sidecar = {'grant_id': grant['id'], 'grant_path': str(grant_path), 'grant_sha256': sha(grant_path),
	'launch_UTC': now.isoformat(), 'remaining_absolute_seconds': remaining,
	'hard_deadline_UTC': grant['hard_deadline_UTC'], 'original_config': str(original),
	'original_config_sha256': sha(original), 'derived_config': str(path),
	'derived_config_sha256': sha(path), 'only_changed_key': 'maximum_seconds',
	'derived_maximum_seconds': derived['maximum_seconds'], 'all280_pins_before': True,
	'collector_sha256': sha(runner), 'argv': argv, 'terminal': False}
destination = request / 'launch-sidecar.json'
destination.write_text(json.dumps(sidecar, indent=2) + '\n')
print(json.dumps(sidecar), flush=True)
result = subprocess.run(argv)
sidecar.update({'terminal': True, 'collector_exit': result.returncode,
	'terminal_UTC': datetime.now(timezone.utc).isoformat(), 'collector_process_returned': True})
destination.write_text(json.dumps(sidecar, indent=2) + '\n')
print(json.dumps({'terminal': True, 'collector_exit': result.returncode,
	'terminal_UTC': sidecar['terminal_UTC'], 'output': str(out)}), flush=True)
raise SystemExit(result.returncode)
