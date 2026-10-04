from pathlib import Path
import hashlib
import subprocess

root = Path('/home/repyt/workspace/a-program-workers/job-evidence')
area = root / 'src/prototype/solver_inputs'
ev = area / 'epochs/job_evidence_e22'
parent = area / 'job_evidence_epoch18'
source = area / 'job_evidence_epoch22'
control = area / 'job_evidence_epoch22_parent_controls'
assert not ev.exists() and not source.exists() and not control.exists()
manifest = area / 'epochs/job_evidence_e18/tested_sources.sha256'
rows = [line.split('  ', 1) for line in manifest.read_text().splitlines()]
assert len(rows) == 156
assert hashlib.sha256(manifest.read_bytes()).hexdigest() == '02241bd2fa031584e176ae16cbcebd825ec834e8e5bea4ea33f22e66ac4ae2ea'
assert subprocess.check_output(['git', 'show', 'a8f52a015f73fe7cfa067c3d5b122bef7e4532b4:src/prototype/artifact_persistence/artifact/source.c'], cwd=root) == (parent / 'src/artifact/source.c').read_bytes()
ev.mkdir()
for overlay in [source, control]:
	overlay.mkdir()
	for digest, name in rows:
		blob = (parent / name).read_bytes()
		assert hashlib.sha256(blob).hexdigest() == digest, name
		path = overlay / name
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_bytes(blob)
	for name in ['Makefile', 'src/Makefile']:
		(overlay / name).write_bytes((parent / name).read_bytes())
	(overlay / 'private_tmp').mkdir()
(ev / 'parent_sources.sha256').write_bytes(manifest.read_bytes())
(ev / 'parent.txt').write_text('Exact isolated published E18 taska8f52a015f73fe7cfa067c3d5b122bef7e4532b4/source15602241bd2; E19/E20/E21/current Main/E10 excluded. Canonical artifact/source.c is an existing explicitly granted AP producer owner file. No shared Git write.\n')
print('Exact E18 parent and private candidate/control156 prepared')
