import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import signal
import subprocess

request = Path('/tmp/a-program-merge-mem2-spine-root-cost-20261003')
request.mkdir()
original = Path('/tmp/ap-performance-mem2-iadt-spine-cost-request-20261003/configuration-proposed.json')
grant = json.loads(Path('/tmp/a-program-merge-mem2-spine-root-cost-grant-20261003.json').read_text())
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(original) == grant['configuration_sha256']
config = json.loads(original.read_text())
assert len(config['input_sha256']) == 279 and len(config['jobs']) == 36
assert not Path('/tmp/ap-performance-mem2-iadt-spine-exclusive-samples-20261003').exists()
for name, expected in config['input_sha256'].items():
	assert sha(Path(name)) == expected, name
assert sha(Path('/tmp/ap-performance-mem2-iadt-spine-cost-request-20261003/expired-grant-terminal.json')) == grant['prior_launcher_refusal_sha256']
active = []
for p in Path('/proc').glob('[0-9]*'):
	try:
		comm = (p / 'comm').read_text().strip()
		if comm in ['pointer-check', 'cc', 'cc1', 'collect2', 'ld', 'clang', 'allocation_coun', 'gdb'] or comm.endswith('_test'):
			active.append({'pid': int(p.name), 'comm': comm})
	except (FileNotFoundError, PermissionError, ProcessLookupError):
		pass
assert not active, active
utc = datetime.timezone.utc
now = datetime.datetime.now(utc)
start = datetime.datetime.fromisoformat(grant['not_before_UTC'].replace('Z', '+00:00'))
deadline = datetime.datetime.fromisoformat(grant['hard_deadline_UTC'].replace('Z', '+00:00'))
assert start <= now < deadline
remaining = (deadline - now).total_seconds()
cap = min(600, math.floor(remaining) - 2)
assert cap > 0
config['maximum_seconds'] = cap
derived = request / 'configuration-derived.json'
derived.write_text(json.dumps(config, indent=2) + '\n')
assert [k for k in config if config[k] != json.loads(original.read_text())[k]] in [[], ['maximum_seconds']]
sidecar = {'grant_id': grant['grant_id'], 'executor': 'Root',
	'actual_launch_UTC': now.isoformat(), 'hard_deadline_UTC': deadline.isoformat(),
	'remaining_absolute_seconds': remaining, 'maximum_seconds': cap,
	'original_sha256': sha(original), 'derived_sha256': sha(derived),
	'only_allowed_changed_key': 'maximum_seconds', 'all279_pins_before': True,
	'no_other_workload_build_children_before': True}
(request / 'launch.json').write_text(json.dumps(sidecar, indent=2) + '\n')
print(json.dumps(sidecar), flush=True)
argv = ['python3', grant['collector'], str(derived), grant['measurement_output'], '--granted-slot', grant['grant_id']]
with (request / 'collector.log').open('wb') as log:
	child = subprocess.Popen(argv, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
	sidecar.update({'collector_pid': child.pid, 'collector_process_group': child.pid, 'argv': argv})
	(request / 'launch.json').write_text(json.dumps(sidecar, indent=2) + '\n')
	try:
		code = child.wait(timeout=max(0.1, (deadline - datetime.datetime.now(utc)).total_seconds() - 0.5))
		forced = False
	except subprocess.TimeoutExpired:
		os.killpg(child.pid, signal.SIGKILL)
		code = child.wait()
		forced = True
terminal = {'grant_id': grant['grant_id'], 'collector_exit': code,
	'terminal_UTC': datetime.datetime.now(utc).isoformat(), 'forced_at_deadline': forced,
	'collector_returned': True}
(request / 'terminal.json').write_text(json.dumps(terminal, indent=2) + '\n')
print(json.dumps(terminal), flush=True)
raise SystemExit(code)
