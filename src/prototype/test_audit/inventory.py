#!/usr/bin/env python3
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def digest(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def logical_lines(path):
	parts = []
	start = 0
	for number, line in enumerate(path.read_text().splitlines(), 1):
		if not parts:
			start = number
		parts.append(line.rstrip('\\').rstrip())
		if line.endswith('\\'):
			continue
		yield start, ' '.join(parts)
		parts = []
	if parts:
		raise ValueError(f'unfinished continuation: {path}')


def property_for(command, target):
	if target == 'check-build-layout':
		return 'HARNESS.BUILD_LAYOUT'
	if command.startswith('mkdir ') or ' -o ' in command:
		return 'HARNESS.BUILD'
	if command.startswith('bash '):
		return 'SCRIPT.' + Path(shlex.split(command)[1]).stem.upper()
	if '--reject' in command:
		return 'SOURCE.REJECTION'
	if '--unsupported' in command:
		return 'SOURCE.UNSUPPORTED'
	if '--equal' in command:
		return 'RESULT.EQUALITY'
	if 'syntax_io_test' in command:
		return 'SYNTAX.ROUNDTRIP'
	if 'parse_files' in command:
		return 'PARSE.CURRENT'
	if 'pointer-check' in command:
		return 'SOURCE.ADMISSION'
	return 'INTERNAL.CONTRACTS_SEE_SOURCE'


def main():
	parser = argparse.ArgumentParser(description='Static recipe inventory; never runs a gate.')
	parser.add_argument('--overlay', required=True, type=Path)
	parser.add_argument('--output', required=True, type=Path)
	args = parser.parse_args()
	repo = Path(__file__).resolve().parents[3]
	overlay = args.overlay.resolve()
	files = {}

	def label(path):
		value = str(path)
		return value.replace(str(overlay), '<overlay>').replace(str(repo), '<repo>')

	def pin(path):
		files[label(path)] = digest(path)

	makefiles = [repo / 'src/Makefile', overlay / 'src/Makefile']
	makefiles += [repo / f'src/prototype/{name}/build.mk' for name in
		('artifact_persistence', 'performance', 'performance_verification', 'surface', 'c_backend')]
	graph = []
	for path in makefiles:
		pin(path)
		for number, line in logical_lines(path):
			if '$(shell' in line or ('$(MAKE)' in line and 'build_layout.sh' not in line):
				raise RuntimeError(f'possible execution in dry-run requires review: {path}:{number}')
			if line.startswith(('include ', '-include ')):
				graph.append(dict(source=label(path), line=number, include=line))
			if line.startswith(('\t', '#', '.')) or ':=' in line:
				continue
			match = re.match(r'([^:]+):\s*(.*)', line)
			if match and any(target.startswith('check') for target in match[1].split()):
				graph.append(dict(source=label(path), line=number,
					targets=match[1].split(), prerequisites=match[2]))

	profiles = [('accepted', repo / 'src/Makefile', ['check-acceptance'])]
	profiles += [('assembled', repo / 'src/prototype/performance_verification/build.mk',
		['check-acceptance', 'check-artifact-transport', 'check-artifact-semantic',
		 'check-artifact-history', 'check-artifact-metrics', 'check-artifact-sorting',
		 'check-artifact-partitions', 'check-artifact-normalization-checkpoint',
		 'check-artifact-source-checkpoint', 'check-artifact-derivation-checkpoint',
		 'check-artifact-definition-checkpoint', 'check-artifact-namespace-frontier',
		 'check-artifact-namespace-body-frontier', 'check-artifact-constructor-checkpoint'])]
	profiles += [(name, repo / f'src/prototype/{name}/build.mk', targets) for name, targets in
		[('surface', ['check-surface']), ('c_backend', ['check-c-backend', 'check-c-link',
		 'check-c-scalar', 'check-c-enum', 'check-c-data', 'check-c-list',
		 'check-c-numeric-list', 'check-c-static-functions', 'check-c-value-records',
		 'check-c-sorting-boundary'])]]
	recipes = []
	with tempfile.TemporaryDirectory(prefix='verification-audit-inventory-') as work:
		stub = Path(work) / 'no_recursive.mk'
		stub.write_text('check-build-layout:\n\t@:\n')
		for profile, makefile, targets in profiles:
			command = ['make', '--no-print-directory', '--trace', '-Bn', '-f', str(makefile),
				'-f', str(stub), f'OVERLAY={overlay}', f'BUILD={work}/build',
				f'CHECKPOINT_TESTS={overlay}/checkpoint_tests/',
				f'ARTIFACT_TESTS={overlay}/artifact_tests/'] + targets
			result = subprocess.run(command, cwd=repo, text=True, capture_output=True, check=True)
			unexpected = [line for line in result.stderr.splitlines() if not
				('overriding recipe for target' in line or 'ignoring old recipe for target' in line)]
			if unexpected:
				raise RuntimeError('\n'.join(unexpected))
			current = None
			for line in result.stdout.splitlines():
				trace = re.match(r"(.+):(\d+): (?:update target|target) '([^']+)'", line)
				if trace:
					current = dict(source=label(trace[1]), line=int(trace[2]),
						target=trace[3].replace(work, '<dry-run>'))
					continue
				if current is None:
					raise RuntimeError(f'unrecognized dry-run output: {line}')
				if current['target'] == 'check-build-layout':
					suite_makefile = (repo if profile == 'accepted' else overlay) / 'src/Makefile'
					layout_line = next(number for number, value in logical_lines(suite_makefile)
						if 'build_layout.sh' in value)
					current = dict(source=label(suite_makefile), line=layout_line, target='check-build-layout')
					line = f'bash {repo if profile == "accepted" else overlay}/tests/build_layout.sh make'
				if '$(MAKE)' in line or re.search(r'\bmake\b', line) and current['target'] != 'check-build-layout':
					raise RuntimeError('new recursive recipe requires manual review')
				normal = label(line).replace(work, '<dry-run>')
				prop = property_for(normal, current['target'])
				if prop == 'HARNESS.BUILD' and ' -o ' in normal:
					command_hash = hashlib.sha256(normal.encode()).hexdigest()
					flags = normal.split(' -I', 1)[0]
					normal = flags + ' [dependencies omitted; regenerate exact command] -o ' + normal.rsplit(' -o ', 1)[1]
					current = dict(**current, expanded_command_sha256=command_hash)
				recipes.append(dict(profile=profile, **current, command=normal, property=prop))

	scripts = {}
	for row in recipes:
		for token in shlex.split(row['command']):
			if token.endswith('.sh'):
				path = Path(token.replace('<repo>', str(repo)).replace('<overlay>', str(overlay)))
				if path.is_file():
					scripts[label(path)] = path
	shell_sources = []
	for name, path in sorted(scripts.items()):
		pin(path)
		loops = [number for number, line in logical_lines(path)
			if re.search(r'\b(for|while|case)\b|<<|\(\)\s*\{', line)]
		shell_sources.append(dict(source=name, sha256=digest(path), dynamic_site_lines=loops))
	groups = collections.defaultdict(list)
	for number, row in enumerate(recipes):
		if not row['property'].startswith('HARNESS.'):
			groups[(row['profile'], row['command'])].append(number)
	duplicates = [dict(profile=key[0], command=key[1], recipe_indices=indices,
		classification='exact command candidate; dependency/assertion key requires review')
		for key, indices in groups.items() if len(indices) > 1]
	for path in sorted((overlay / 'src').rglob('*')):
		if path.is_file() and (path.suffix in ('.c', '.h') or path.name == 'Makefile'):
			pin(path)
	for path in (repo / 'tests/compatibility.tsv', repo / 'tests/syntax_exclusions.tsv',
		repo / 'tests/inventory.sh', repo / 'README.md'):
		pin(path)
	legacy = collections.defaultdict(list)
	fixture_root = repo / 'archive/legacy/src/prototype/tests/fixtures'
	cases = (repo / 'tests/compatibility.sh').read_text().split("done <<'CASES'\n", 1)[1].split('\nCASES', 1)[0]
	for row in cases.splitlines():
		words = row.split()
		path = (fixture_root / (words[1] + '.p')).resolve()
		if 'archive/legacy/' in str(path):
			legacy[path].append('compatibility.sh CASES: explicit legacy admission/rejection; selected results remain distinct')
	for number, row in enumerate((repo / 'tests/compatibility.tsv').read_text().splitlines(), 1):
		words = row.split('\t')
		if words[0] == 'program':
			path = repo / words[2]
			pin(path)
			if 'archive/legacy/' in str(path):
				legacy[path].append(f'syntax_inventory.sh / compatibility.tsv:{number}: legacy parse contract only, review_pending is historical metadata')
	for name in ('compatibility.sh', 'image_cli.sh', 'retained_quicksort.sh', 'cli.sh'):
		for number, line in logical_lines(repo / 'tests' / name):
			for match in re.finditer(r'(?:\$fixtures/|archive/legacy/src/prototype/tests/fixtures/)([\w/-]+\.p)', line):
				path = fixture_root / match[1]
				legacy[path].append(f'{name}:{number}: ' + ('current all-refuted Match pending/image limitation' if name == 'cli.sh' else
					('retained root/origin and image lifecycle regression' if name in ('image_cli.sh', 'retained_quicksort.sh') else
					 'current imported-provider/result/image/negative regression using preserved legacy source')))
	legacy_rows = []
	for path, uses in sorted(legacy.items()):
		if not path.is_file():
			raise FileNotFoundError(path)
		pin(path)
		legacy_rows.append(dict(path=label(path), uses=sorted(set(uses)),
			decision='keep; any semantic replacement requires explicit review; syntax cohort retirement changes compatibility coverage'))
	output = dict(revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip(),
		method='GNU Make forced traced dry-run with recursive build-layout command restored; static source graph and shell sources',
		limits=['No binary built or gate executed; no runtime coverage or cost claim.',
		 'Repeated dependencies share a target within each Make invocation; separate Make processes may repeat it.',
		 'Shell/C effective invocation expansion remains incomplete outside the manually reviewed pilot.',
		 'Graph prerequisites retain Make expressions; dry-run recipes expand them.',
		 'Source hashes pin this assembly, not historical gate binaries or toolchains.'],
		profiles={name: dict(makefile=label(makefile), targets=targets,
			owner='Merge' if name == 'accepted' else ('Job/Evidence + performance' if name == 'assembled' else name),
			lifecycle='current-contract/harness' if name == 'accepted' else 'prototype qualification',
			cost='unmeasured', nested_coverage='unenumerated shell/C internals; pilot manually reviewed')
			for name, makefile, targets in profiles},
		graph=graph, recipes=recipes, shell_sources=shell_sources, legacy_dependencies=legacy_rows,
		exact_command_candidates=duplicates, hashes=files)
	sections = []
	for key, value in output.items():
		if isinstance(value, list):
			body = ',\n'.join(json.dumps(row, sort_keys=True) for row in value)
			sections.append(json.dumps(key) + ': [\n' + body + '\n]')
		else:
			sections.append(json.dumps(key) + ': ' + json.dumps(value, sort_keys=True))
	args.output.write_text('{\n' + ',\n'.join(sections) + '\n}\n')
	print(json.dumps(dict(profiles=dict(collections.Counter(row['profile'] for row in recipes)),
		shell_sources=len(shell_sources), exact_command_groups=len(duplicates), output=str(args.output))))


if __name__ == '__main__':
	main()
