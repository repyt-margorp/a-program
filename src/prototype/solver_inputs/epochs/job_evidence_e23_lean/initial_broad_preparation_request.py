from pathlib import Path
import hashlib
import json

root = Path('/home/repyt/workspace/a-program-workers/job-evidence')
area = root / 'src/prototype/solver_inputs'
ev = area / 'epochs/job_evidence_e23'
source = area / 'job_evidence_epoch23'
previous = area / 'epochs/job_evidence_e22'
overlay = area / 'job_evidence_epoch22'
assert not (ev / 'broad_request.json').exists()
rows = (previous / 'overlay_inputs.sha256').read_text().splitlines()
assert len(rows) == 471
for row in rows:
	digest, name = row.split('  ', 1)
	blob = (overlay / name).read_bytes()
	assert hashlib.sha256(blob).hexdigest() == digest, name
	path = source / name
	if path.exists():
		assert path.read_bytes() == blob, name
	else:
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_bytes(blob)
(ev / 'overlay_inputs.sha256').write_bytes((previous / 'overlay_inputs.sha256').read_bytes())
request = json.loads((previous / 'broad_request.json').read_text())
request['commands'] = [arg.replace('job_evidence_epoch22', 'job_evidence_epoch23') for arg in request['commands']]
request['source_manifest_sha256'] = hashlib.sha256((ev / 'tested_sources.sha256').read_bytes()).hexdigest()
request['reuse'] = 'None: new private O2 directory; focused wrapper binary is not substituted. All acceptance/semantic/seven checkpoint recipes unskipped.'
request['parent_task'] = 'Exact private E22 source1566491c060/lean31manifest233fa25a, locally full-qualified/unpublished; on E18taska8f52a0, E19/E20/E21/current Main/E10 excluded. Supersedes preparation-time broad-pending status.'
request['environment'] = {'TMPDIR': str(source / 'private_tmp')}
request['rerun_provenance'] = 'First actual E23 broad recipe; actual471 inputs exact. Corrected focused12 preserves original11/canonical-label provenance. Parent two retention134/new0; E22 locally full-qualified/unpublished, not published or current joint.'
(ev / 'broad_request.json').write_text(json.dumps(request, indent=2) + '\n')
helper = Path('/tmp/job-evidence-e22-broad.py')
assert not helper.exists()
helper.write_text(Path('/tmp/job-evidence-e21-broad.py').read_text().replace('job_evidence_e22', 'job_evidence_e23').replace('job_evidence_epoch22', 'job_evidence_epoch23'))
print('Exact actual471 and unskipped E22 broad request prepared')
