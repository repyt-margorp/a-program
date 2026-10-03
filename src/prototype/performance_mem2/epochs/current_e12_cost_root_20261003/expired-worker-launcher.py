#!/usr/bin/env python3
"""Launch only the exact granted MEM2 jobs within the absolute slot."""
import datetime
import hashlib
import json
import math
from pathlib import Path
import subprocess

request = Path('/tmp/ap-performance-mem2-iadt-spine-cost-request-20261003')
original = request / 'configuration-proposed.json'
expected = '8cc0b7533615f954a4f59bf1b7bfa4e9f2ac597aac4b1ef78656da0fba711236'
assert hashlib.sha256(original.read_bytes()).hexdigest() == expected
configuration = json.loads(original.read_text())
assert len(configuration['jobs']) == 36
assert len(configuration['input_sha256']) == 279
for name, pin in configuration['input_sha256'].items():
	assert hashlib.sha256(Path(name).read_bytes()).hexdigest() == pin, name
utc = datetime.timezone.utc
now = datetime.datetime.now(utc)
start = datetime.datetime(2026, 10, 3, 18, 0, tzinfo=utc)
deadline = datetime.datetime(2026, 10, 3, 18, 10, tzinfo=utc)
assert now >= start
remaining = (deadline - now).total_seconds()
cap = min(600, math.floor(remaining) - 2)
assert cap > 0, 'Grant expired; no launch permitted'
configuration['maximum_seconds'] = cap
derived = request / 'configuration-actual.json'
assert not derived.exists()
derived.write_text(json.dumps(configuration, indent=2) + '\n')
sidecar = {'grant_id': 'MEM2IADT-E12-20261003T180000Z-600',
	'actual_launch_UTC': now.isoformat(), 'remaining_absolute_seconds': remaining,
	'maximum_seconds': cap, 'original_sha256': expected,
	'derived_sha256': hashlib.sha256(derived.read_bytes()).hexdigest(),
	'only_changed_configuration_key': 'maximum_seconds',
	'hard_deadline_UTC': deadline.isoformat(), 'all279_pins_before': True}
(request / 'launch.json').write_text(json.dumps(sidecar, indent=2) + '\n')
print(json.dumps(sidecar), flush=True)
collector = '/tmp/ap-performance-measure-gnu-time-e6-20261003.py'
output = '/tmp/ap-performance-mem2-iadt-spine-exclusive-samples-20261003'
result = subprocess.run(['python3', collector, str(derived), output,
	'--granted-slot', sidecar['grant_id']])
terminal = {'grant_id': sidecar['grant_id'], 'collector_exit': result.returncode,
	'terminal_UTC': datetime.datetime.now(utc).isoformat(),
	'collector_and_sequential_workload_returned': True}
(request / 'terminal-launcher.json').write_text(json.dumps(terminal, indent=2) + '\n')
print(json.dumps(terminal), flush=True)
raise SystemExit(result.returncode)
