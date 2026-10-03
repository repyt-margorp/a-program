#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path
import shutil
import statistics

worker = Path('/home/repyt/workspace/a-program-workers/performance')
root = Path('/tmp/ap-performance-mem1-v3-e12-exclusive-samples-20261003')
request = Path('/tmp/ap-performance-mem1-v3-current-e12-cost-request-20261003')
publication = worker / 'src/prototype/performance_mem1/epochs/current_e12_cost_20261003'
publication.mkdir()
data = json.loads((root / 'measurements.json').read_text())
assert data['complete'] and data['all_planned_samples_passed'] and len(data['samples']) == 36
assert json.loads((root / 'terminal.json').read_text())['no_matching_live_children']
groups = {}
for row in data['samples']:
	assert row['exit'] == 0 and row['passed'] and not row['timeout']
	assert row['charged_steps'] == data['configuration']['jobs'][row['index']]['expected_charged_steps']
	groups.setdefault((row['case'], row['variant']), []).append(row)


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def metrics(rows, key):
	values = [row[key] for row in sorted(rows, key=lambda row: row['repetition'])]
	assert len(values) == 3
	return {'repetitions': values, 'median': statistics.median(values),
		'minimum': min(values), 'maximum': max(values)}


cases = list(dict.fromkeys(row['case'] for row in data['samples']))
analysis = {'grant_id': data['granted_slot'], 'terminal36_pass0': True,
	'all36_charged_steps_exact': True, 'failed_incomplete_timeout_censored': 0,
	'baseline_source128': data['configuration']['baseline_source128'],
	'candidate_source128': data['configuration']['candidate_source128'],
	'only_eval_c_h_differ': True, 'cases': [],
	'original_configuration_sha256': sha(request / 'configuration-final-proposed.json'),
	'derived_configuration_sha256': data['configuration_sha256'],
	'launch_sidecar_sha256': sha(request / 'launch-sidecar.json'),
	'measurements_sha256': sha(root / 'measurements.json'),
	'limits': data['configuration']['limits'], 'full_Goal_active': True,
	'accepted_promotion_or_Main_action_by_worker': False}
for case in cases:
	row = {'case': case, 'charged_steps_each_sample': groups[case, 'baseline'][0]['charged_steps']}
	for variant in ('baseline', 'captured-head-v3'):
		row[variant] = {key: metrics(groups[case, variant], key) for key in
			('wall_seconds_GNU_time', 'wall_seconds_launcher', 'maximum_RSS_KiB')}
	for key in ('wall_seconds_GNU_time', 'maximum_RSS_KiB'):
		before = row['baseline'][key]['median']
		after = row['captured-head-v3'][key]['median']
		row[key + '_median_delta'] = after - before
		row[key + '_median_percent_change'] = None if not before else 100 * (after - before) / before
		row[key + '_ranges_overlap'] = max(row['baseline'][key]['minimum'], row['captured-head-v3'][key]['minimum']) <= min(row['baseline'][key]['maximum'], row['captured-head-v3'][key]['maximum'])
	analysis['cases'].append(row)
large = next(row for row in analysis['cases'] if row['case'] == 'trees400')
assert not large['wall_seconds_GNU_time_ranges_overlap'] and not large['maximum_RSS_KiB_ranges_overlap']
analysis['tree400_finding'] = 'Exact matched workload improves both time and actual peak RSS; three ranges do not overlap. Not a universal performance claim.'
analysis['quick_finding'] = 'Timing ranges overlap at0.61s; no established speed claim. Peak RSS median grows492KiB(+0.29%) in these samples; no QuickSort memory improvement.'
analysis['list_finding'] = 'GNU wall is0.00s throughout; launcher/startup and RSS ranges overlap. No evaluator speed or memory improvement claim.'
analysis['remaining_memory_work'] = 'Tree median remains about978.3MiB. Attribute remaining argument/environment retention before another deletion; preserve required closures, opaque state, fuel and resume.'
(root / 'analysis.json').write_text(json.dumps(analysis, indent=2) + '\n')
lines = ['# MEM1 v3: Matched Current E12 Costs', '',
	'Exact current prototype producer, baseline `12026914` and v3 `0deb36a7`; only',
	'`eval.c`/`eval.h` differ. All36 samples pass with exact expected charged steps.',
	'Three fresh processes per variant/case, sequential; repetition2 reverses paired',
	'order. Existing GNU time collector, warm filesystem, no cache flush.', '',
	'| Case | Charged steps | Baseline wall median [range], s | V3 wall median [range], s | Baseline peak RSS median [range], KiB | V3 peak RSS median [range], KiB |',
	'| --- | ---: | ---: | ---: | ---: | ---: |']
for row in analysis['cases']:
	values = []
	for key in ('wall_seconds_GNU_time', 'maximum_RSS_KiB'):
		for variant in ('baseline', 'captured-head-v3'):
			m = row[variant][key]
			fmt = (lambda value: f'{value:.2f}') if key.startswith('wall') else (lambda value: str(value))
			values.append(fmt(m['median']) + ' [' + fmt(m['minimum']) + '–' + fmt(m['maximum']) + ']')
	lines.append('| ' + row['case'] + ' | ' + str(row['charged_steps_each_sample']) + ' | ' + ' | '.join(values) + ' |')
lines += ['',
	'Tree400 wall median5.33→4.92s (−7.69%); peak RSS1891928→1001764KiB',
	'(−47.05%, about1847.6→978.3MiB). Both three-sample ranges are disjoint.',
	'Tree64 also has disjoint time/RSS ranges. Tree4/16 time ranges overlap and',
	'are close to GNU time\'s0.01s resolution; their RSS reductions are observed.',
	'QuickSort timing overlaps at0.61s and its RSS median increases492KiB (+0.29%).',
	'List GNU wall is0.00s for all runs; launcher/startup dominates and RSS overlaps.',
	'Full three-repetition values, including launcher wall, are in `analysis.json`.', '',
	'This is actual process peak RSS/time for these concrete AP source/proof/helper',
	'workloads. Includes parsing, synthesis, conversion, source-provider import and',
	'process/wrapper startup. RSS is GNU time\'s wrapped-command maximum, not a sum',
	'of simultaneously resident processes or semantic live bytes. Allocator/kernel',
	'effects are included. No native systems were rerun; their different proof/helper',
	'work prevents a universal ranking. These prototype costs do not establish',
	'accepted-only values. Selected-cut capacity/cumulative allocation findings',
	'remain separate from these actual measurements; new WHNF aligned overhead',
	'and the small-workload cost remain visible.', '',
	'Grant `MEM1V3-E12-20261003T154600Z-600`:15:46–15:56UTC. Actual launch',
	'15:48:26.754134UTC; remaining453.245866s, derived maximum451s. The original',
	'configuration `a3335c5e` differs from `f51097b9` only in maximum_seconds.',
	'Collector terminates0 at15:49:08.050690UTC. All280 pins and every output/metrics',
	'hash verify. All36 expected counts agree; zero failed, timeout, incomplete or',
	'censored samples. Terminal stopped-child notice precedes this analysis.', '',
	'Original strict3, Core/Identity observers, f3 seven sanitizer failures, rejected',
	'MEM1v1 inline-state and later v2 materialized-callback UAF remain separately',
	'preserved. E11/E12 corrected qualification remains frozen without MEM1. V3',
	'and its current E12 pair pass callback, fuel, raw cuts, step0 and split-image',
	'controls; public52 images/full verdict-fuel TSV equal the matched parent.',
	'No new full-suite, accepted promotion, Main merge or Goal-closure claim.', '',
	'Tree peak remains about978MiB. Remaining owned memory/deletion work is active;',
	'generic argument/environment recycling is not justified by reachability guesses.',
	'Further profiling/heavy correctness and any new measurements follow the',
	'coordinator release/slot rules. Published epoch bytes remain immutable.', '']
(root / 'report.md').write_text('\n'.join(lines))
for p in root.iterdir():
	if p.is_file():
		shutil.copy2(p, publication / p.name)
for sample in root.glob('sample-*'):
	shutil.copytree(sample, publication / sample.name)
for name in ('configuration-proposed.json', 'configuration-final-proposed.json',
	'configuration-granted-derived.json', 'launch-sidecar.json', 'request.json',
	'request-final.json', 'correctness.json'):
	shutil.copy2(request / name, publication / name)
shutil.copytree(request / 'inputs', publication / 'inputs')
for p in request.glob('*-correctness.log'):
	shutil.copy2(p, publication / p.name)
shutil.copy2('/tmp/a-program-merge-mem1-v3-e12-exclusive-grant-20261003.json', publication / 'grant.json')
for label, source in [('baseline', Path('/tmp/ap-performance-current-joint-898463e-bd1ddf3-6715af2-d22be9f-cb959eb-20261003')),
	('candidate', Path('/tmp/ap-performance-mem1-v3-current-e12-20261003'))]:
	shutil.copy2(source / 'source.sha256', publication / (label + '-source.sha256'))
shutil.copy2(Path(__file__), publication / 'analyze.py')
shutil.copy2('/tmp/ap-performance-launch-mem1-exclusive.py', publication / 'launch-provenance.py')
records = [(sha(p), str(p.relative_to(worker))) for p in sorted(publication.rglob('*')) if p.is_file()]
manifest = publication / 'manifest.sha256'
assert not manifest.exists()
manifest.write_text(''.join(expected + '  ' + relative + '\n' for expected, relative in records))
for expected, relative in records:
	assert sha(worker / relative) == expected
print(json.dumps({'publication': str(publication), 'files_including_manifest': len(records) + 1,
	'manifest_sha256': sha(manifest), 'analysis_sha256': sha(root / 'analysis.json'),
	'wall_median_delta_percent_tree400': large['wall_seconds_GNU_time_median_percent_change'],
	'RSS_median_delta_percent_tree400': large['maximum_RSS_KiB_median_percent_change']}, indent=2))
