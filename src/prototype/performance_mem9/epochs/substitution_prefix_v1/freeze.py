#!/usr/bin/env python3
"""Freeze one qualified evaluator scan deletion without changing Git or owners."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

owner = Path(__file__).resolve().parent
worker = owner.parents[2]
trial = owner / 'private/substitution_prefix_v1_20261003'
parent = Path('/tmp/ap-performance-mem5-materialized-fields-current-e12-20261003')
epoch = owner / 'epochs/substitution_prefix_v1'


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_manifest(root, name):
	lines = (root / name).read_text().splitlines()
	for line in lines:
		expected, relative = line.split('  ', 1)
		assert sha(root / relative) == expected, relative
	return len(lines)


assert verify_manifest(parent, 'source.sha256') == 128
assert verify_manifest(trial, 'source.sha256') == 128
focused = json.loads((trial / 'focused-summary.json').read_text())
affected = json.loads((trial / 'affected-summary.json').read_text())
assert focused['terminal'] and focused['records'] == 454 and focused['failed'] == 0
assert affected['terminal'] and affected['new_failures'] == 0 and len(affected['records']) == 10
assert affected['original_strict_recipe_failures'] == 1 and affected['original_strict_reload_failures'] == 3
assert affected['public52_images_equal'] and affected['full_verdict_fuel_TSV_equal']
for summary in (focused, affected):
	for path, expected in summary['pins'].items():
		assert sha(Path(path)) == expected, path
changed = []
for line in (trial / 'source.sha256').read_text().splitlines():
	_, relative = line.split('  ', 1)
	if sha(parent / relative) != sha(trial / relative):
		changed.append(relative)
assert changed == ['src/eval.c']
epoch.mkdir(parents=True, exist_ok=False)
helpers = ['assemble.py', 'build.mk', 'substitution_prefix_test.c',
	'qualify_focused.py', 'qualify_affected.py', 'freeze.py']
for name in helpers:
	shutil.copyfile(owner / name, epoch / name)
shutil.copyfile(trial / 'substitution_prefix.patch', epoch / 'applied.patch')
shutil.copyfile(trial / 'src/eval.c', epoch / 'eval.c')
for name in ('source.sha256', 'parent-source.sha256', 'focused-summary.json', 'affected-summary.json'):
	shutil.copyfile(trial / name, epoch / name)
source_paths = (trial / 'source.sha256').read_text().splitlines()
styles = []
for path in [trial / 'src/eval.c', *[owner / name for name in helpers]]:
	bad = [line for line, value in enumerate(path.read_text().splitlines(), 1)
		if value.startswith(' ') and value.strip() and not value.lstrip().startswith('*')]
	assert not bad, (path, bad)
	styles.append({'path': str(path), 'sha256': sha(path), 'leading_tabs': True})
substitution_trace = worker / 'src/prototype/performance_mem8/private/substitution_prefixes_v1_20261003'
sync_trace = worker / 'src/prototype/performance_mem8/private/synchronous_calls_v1_20261003'
trace_pins = {str(root / 'verification.sha256'): sha(root / 'verification.sha256')
	for root in (substitution_trace, sync_trace)}
for root in (substitution_trace, sync_trace):
	assert verify_manifest(root, 'verification.sha256') == 10
analysis = {'issue': 56, 'pull_request': 58,
	'implemented': 'Delete the second identity-prefix scan by detecting its prefix while validating every binding.',
	'only_runtime_change': changed, 'parent_source128': sha(parent / 'source.sha256'),
	'candidate_source128': sha(trial / 'source.sha256'), 'canonical_patch_sha256': sha(epoch / 'applied.patch'),
	'local': {'focused_records': 454, 'failed': 0, 'totals': focused['totals'],
		'all_cuts_per_variant_build': 22, 'fresh_reads': 352,
		'initial_projection_reads': focused['initial_projection_reads'],
		'affected_records': 10, 'new_failures': 0, 'original_strict_recipe_failures': 1,
		'original_strict_reload_failures': 3, 'public_images_equal': 52,
		'completed_charged_steps': affected['completed_charged_steps']},
	'joint_latest_Main': 'Not freshly qualified; this exact historical common-producer epoch excludes MEM6/MEM7.',
	'historical_attribution': {'source128': '81c3ad27dfd01aef14ad8e977d1b783cc3636368b6e5afcab0840d07d055e541',
		'trace_pins': trace_pins, 'redundant_reader_calls': {'list': 313, 'imported-local-sorted': 844303, 'trees4': 8123},
		'synchronous_readback_and_unshared_substitution_calls': 0},
	'no_new_persistent_storage_owner_graph_index_layout_allocator_codec_or_authority': True,
	'new_local_state': 'One size_t identity-prefix counter during validation; no persistent owner/index overhead.',
	'limits': ['Stable synchronous binding-reader contract; every trailing input is still validated.',
		'Later identities can shadow earlier nonidentities and remain in the captured environment.',
		'Charged traversal, shared request construction, input snapshots, status and state codecs are unchanged.',
		'The removed read counts do not establish net wall-time or actual RSS gains.',
		'No comparative samples or new full-acceptance/other-six-checkpoint claim.',
		'Actual peak-memory work and deferred allocator/compact-root/beta owner contracts remain open.'],
	'style': styles, 'comments_and_documentation_language': 'English',
	'full_goal_complete': False, 'accepted_promotion': False}
(epoch / 'analysis.json').write_text(json.dumps(analysis, indent=2) + '\n')
known = {'parent_known_failures': str(parent / 'known-failures.json'),
	'parent_known_failures_sha256': sha(parent / 'known-failures.json'),
	'original_observer_failures': 2, 'original_SAN_failures': 7, 'strict_reload_failures': 3,
	'MEM6_raw_pair_and_parent_byte_failures': 'Preserved separately; held epoch excluded.',
	'MEM7_unchanged_codec_failures': 4, 'MEM7_runtime': 'Review-only epoch excluded.',
	'padding_beta_copy_setup_and_rejected_runtime_records': 'Preserved in separate immutable manifests.',
	'new_failure_or_test_outcome_waiver': False,
	'public_partition_raw_exit': 1, 'public_partition_log': str(trial / 'affected/public-partitions.log'),
	'public_partition_log_sha256': sha(trial / 'affected/public-partitions.log')}
(epoch / 'known-failures.json').write_text(json.dumps(known, indent=2) + '\n')
publication = {'branch': 'parallel/performance-20261003',
	'parent_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=worker, text=True).strip(),
	'commit_message': 'prototype: combine substitution validation and identity scan',
	'publication': 'Delegated exact task-branch publication only, after Root review.',
	'Main_integration_or_accepted_promotion': 'Root only; not requested by this freeze.',
	'full_goal_complete': False}
assert publication['parent_commit'] == '27a35a1344639687a2b89ba05886d5634c3d090d'
(epoch / 'publication.json').write_text(json.dumps(publication, indent=2) + '\n')
for name in helpers:
	assert sha(owner / name) == sha(epoch / name)
for root in (parent, trial):
	assert verify_manifest(root, 'source.sha256') == 128
private_files = [path for path in sorted(trial.rglob('*')) if path.is_file()]
(trial / 'verification.sha256').write_text(''.join(sha(path) + '  '
	+ str(path.relative_to(trial)) + '\n' for path in private_files))
(epoch / 'private-evidence.json').write_text(json.dumps({'path': str(trial),
	'manifest': str(trial / 'verification.sha256'),
	'manifest_sha256': sha(trial / 'verification.sha256'), 'records': len(private_files),
	'all_workload_children_stopped': True}, indent=2) + '\n')
files = [path for path in sorted(epoch.iterdir()) if path.is_file()]
(epoch / 'manifest.sha256').write_text(''.join(sha(path) + '  '
	+ path.name + '\n' for path in files))
print(json.dumps({'epoch': str(epoch), 'manifest_records': len(files),
	'manifest_sha256': sha(epoch / 'manifest.sha256'), 'private_records': len(private_files),
	'private_manifest_sha256': sha(trial / 'verification.sha256'),
	'parent_commit': publication['parent_commit'], 'commit_message': publication['commit_message']}, indent=2))
