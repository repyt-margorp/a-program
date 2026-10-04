#!/usr/bin/env python3
"""Copy the exact published MEM5 producer for a separate scan deletion."""
import hashlib
from pathlib import Path
import shutil

owner = Path(__file__).resolve().parent
parent = Path('/tmp/ap-performance-mem5-materialized-fields-current-e12-20261003')
trial = owner / 'private/substitution_prefix_v1_20261003'
manifest = parent / 'source.sha256'
assert hashlib.sha256(manifest.read_bytes()).hexdigest() == '2344ad41d6db32ceddb5dbc9419aa8da97fcaa00253e075f5ff96b8f72f93b13'
trial.mkdir(parents=True, exist_ok=False)
for line in manifest.read_text().splitlines():
	expected, name = line.split('  ', 1)
	path = parent / name
	assert hashlib.sha256(path.read_bytes()).hexdigest() == expected, name
	target = trial / name
	target.parent.mkdir(parents=True, exist_ok=True)
	shutil.copyfile(path, target)
shutil.copyfile(manifest, trial / 'parent-source.sha256')
shutil.copyfile(parent / 'src/Makefile', trial / 'src/Makefile')
print('Copied exact128 MEM5 runtime files; MEM6/MEM7 and live owners excluded.')
