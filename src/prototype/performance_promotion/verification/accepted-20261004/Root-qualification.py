from pathlib import Path
import datetime, hashlib, json, os, re, signal, subprocess, sys

s = Path(__file__).resolve().parent
p = s / 'performance-approved-promotion'
old = s / 'performance-approved-frame-only'
out = s / 'performance-approved-full'
worker = Path('/home/repyt/workspace/a-program-workers/performance/src/prototype/performance_critical_frame')
private = worker / 'private'
focused = private / 'accepted94_order_stateless_frames_v1_focused_20261004'
completed = private / 'accepted94_order_stateless_frames_v1_completed_20261004'
env = dict(os.environ, TMPDIR=str(out / 'tmp'), ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
(out / 'tmp').mkdir(exist_ok=True)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
now = lambda: datetime.datetime.now(datetime.timezone.utc).isoformat()

def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')

def runner(directory):
    directory.mkdir(exist_ok=False)
    records = []
    def run(label, argv, expected=0, limit=360):
        argv = list(map(str, argv))
        row = dict(label=label, argv=argv, started_UTC=now(), expected_exit=expected)
        log = directory / (label + '.log')
        with log.open('wb') as stream:
            child = subprocess.Popen(argv, env=env, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
            row['censored'] = False
            try:
                child.wait(timeout=limit)
            except subprocess.TimeoutExpired:
                row['censored'] = True
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
        row.update(exit=child.returncode, log_sha256=sha(log), finished_UTC=now())
        records.append(row)
        save(directory / 'records.json', records)
        assert child.returncode == expected and not row['censored'], row
        return log.read_text()
    return run, records

mode = sys.argv[1]
if mode == 'san':
    run, records = runner(out / 'additional-san')
    for row in json.loads((old / 'additional-results.json').read_text())[1:]:
        run(row['label'], row['argv'])
    save(out / 'additional-san' / 'summary.json', dict(terminal=True, commands=len(records), unexpected_failures=0))
elif mode == 'fuels':
    run, records = runner(out / 'completed-fuels')
    rows = []
    for row in json.loads((old / 'completed-fuels.json').read_text()):
        text = run(row['case'], row['argv'])
        match = re.fullmatch(r'done steps=(\d+)\s*', text)
        assert match and int(match[1]) == row['expected_fuel_from_current_accepted_parent'], text
        rows.append(dict(case=row['case'], actual_fuel=int(match[1]), equal=True, expected_fuel=row['expected_fuel_from_current_accepted_parent']))
    save(out / 'completed-fuels' / 'summary.json', dict(terminal=True, commands=len(records), fuels=rows))
elif mode == 'profiles':
    run, records = runner(out / 'public-profiles')
    images, inert, resumes = [], [], []
    original = json.loads((completed / 'records.json').read_text())
    writers = [row for row in original if row['label'].startswith('write-save') and '-candidate-' in row['label']]
    assert len(writers) == 12
    for row in writers:
        argv = row['argv'].copy()
        profile, cut = argv[3], int(argv[2])
        stem = profile[2:] + '-' + str(cut)
        image = out / 'public-profiles' / (stem + '.a')
        argv[0], argv[4] = str(old / 'build-o2/pointer-check'), str(image)
        run('write-' + stem, argv, row['expected_exit'])
        parent = completed / (profile[2:] + '-parent-' + str(cut) + '.a')
        equal = image.read_bytes() == parent.read_bytes()
        images.append(dict(profile=profile, cut=cut, equal=equal, image_sha256=sha(image), parent_sha256=sha(parent)))
        assert equal, images[-1]
        for flavor in ['o2', 'san']:
            binary = old / ('build-' + flavor) / 'pointer-check'
            resaved = out / 'public-profiles' / (stem + '-inert-' + flavor + '.a')
            text = run('inert-' + stem + '-' + flavor, [binary, '--load', '--steps', '0', profile, resaved, image], 3)
            equal = image.read_bytes() == resaved.read_bytes()
            inert.append(dict(profile=profile, cut=cut, flavor=flavor, equal=equal, writer_sha256=sha(image), resave_sha256=sha(resaved)))
            save(out / 'public-profiles/inert.json', inert)
            assert text.strip() == 'pending steps=0' and equal, inert[-1]
            text = run('resume-' + stem + '-' + flavor, [binary, '--load', image])
            expected = next(r for r in original if r['label'] == 'resume-candidate-' + flavor + '-' + profile[2:] + '-candidate-' + str(cut))
            assert sha(out / 'public-profiles' / ('resume-' + stem + '-' + flavor + '.log')) == expected['log_sha256']
            resumes.append(dict(profile=profile, cut=cut, flavor=flavor, output=text.strip(), matches_current_accepted_parent=True))
        save(out / 'public-profiles/summary.json', dict(terminal=False, images=images, inert=inert, resumes=resumes))
    save(out / 'public-profiles/summary.json', dict(terminal=True, commands=len(records), image_pairs=12, inert_rows=24, resumes=24, all_bytes_verdicts_fuels_equal=True, images=images, inert=inert, resume_rows=resumes))
elif mode == 'partitions':
    run, records = runner(out / 'public-partitions')
    run('strict-original-controls', ['bash', p / 'src/prototype/image_audit/partition_fuel.sh', old / 'build-o2/pointer-check', p / 'examples/09_list_induction.p', out / 'public-partitions/results', 'ordinary'], 1)
    fresh = out / 'public-partitions/results'
    baseline = old / 'public-partitions'
    relevant = [f for f in baseline.rglob('*') if f.is_file() and f.suffix in ['.a', '.tsv']]
    assert len([f for f in relevant if f.suffix == '.a']) == 52
    for f in relevant:
        other = fresh / f.relative_to(baseline)
        assert f.read_bytes() == other.read_bytes(), (f, other)
    save(out / 'public-partitions/summary.json', dict(terminal=True, strict_exit=1, original_three_failures_unwaived=True, images52_and_full40_TSV_equal=True))
elif mode == 'raw':
    run, records = runner(out / 'raw-cuts')
    for row in json.loads((old / 'raw-build-results.json').read_text()):
        run('build-' + row['flavor'], row['argv'])
    originals = json.loads((old / 'raw-cuts/records.json').read_text())
    for row in originals:
        argv = [a.replace(str(old / 'raw-cuts'), str(out / 'raw-cuts')) for a in row['argv']]
        run(row['label'], argv)
    pair_rows, inert_rows = [], []
    for test in ['fold_spine_test', 'materialized_fields_test']:
        for image in sorted((out / 'raw-cuts').glob(test + '-*.image')):
            if '-inert-' in image.name: continue
            base = focused.parent / 'accepted94_order_stateless_frames_v1_cuts_20261004' / (test + '-parent-' + image.name[len(test)+1:])
            pair_rows.append(dict(image=image.name, equal=image.read_bytes()==base.read_bytes(), candidate_sha256=sha(image), parent_sha256=sha(base)))
            for flavor in ['o2', 'san']:
                resaved = image.with_name(image.stem + '-inert-' + flavor + '.image')
                inert_rows.append(dict(image=image.name, flavor=flavor, equal=image.read_bytes()==resaved.read_bytes(), writer_sha256=sha(image), resave_sha256=sha(resaved)))
    assert len(records) == 1154 and len(pair_rows)==164 and len(inert_rows)==328
    save(out / 'raw-cuts/summary.json', dict(terminal=True, build_commands=2, runtime_commands=1152, semantic_fuel_SAN_unexpected_failures=0, paired=164, paired_byte_failures_unwaived=sum(not r['equal'] for r in pair_rows), inert=328, candidate_inert_byte_failures_unwaived=sum(not r['equal'] for r in inert_rows), pairs=pair_rows, inert_rows=inert_rows, no_original_failure_waiver=True))
else:
    raise AssertionError(mode)
print(mode, 'terminal', flush=True)
