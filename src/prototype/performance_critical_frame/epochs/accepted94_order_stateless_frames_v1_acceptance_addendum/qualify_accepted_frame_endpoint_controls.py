"""Refresh unchanged cross-system AP endpoint controls without collecting costs."""
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess


owner = Path(__file__).resolve().parent
focused = owner / "private/accepted94_order_stateless_frames_v1_focused_20261004"
source = owner / "private/accepted94_order_stateless_frames_v1_20261004"
prior = owner.parent / "performance_cross/results/ap-e1-controls.json"
output = owner / "private/accepted94_order_stateless_frames_v1_endpoints_20261004"
pins = {}
records = []


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def pin(path, expected=None):
	digest = sha(path)
	assert expected is None or digest == expected, path
	pins[str(path)] = digest
	return digest


def save(name, value):
	(output / name).write_text(json.dumps(value, indent=2) + "\n")


pin(Path(__file__))
pin(prior)
cases = json.loads(prior.read_text())["checks"]
assert [row["case"] for row in cases] == ["trees_2", "false_bool", "false_tree"]
assert all(row["qualified"] for row in cases)
for row in cases:
	pin(Path(row["command"][-1]), row["source_sha256"])
sources = {}
for variant, expected in [("parent", "930997b62db85814718d0d5ac151f392bfae243a447d5d0d5f2adbde66cba5ae"),
	("candidate", "c20635425ff3d587fe939f62970d3d6370941013fcb2e85ee0da5e10c4a08907")]:
	manifest = source / variant / "source.sha256"
	sources[variant] = pin(manifest, expected)
	rows = [line.split("  ", 1) for line in manifest.read_text().splitlines()]
	assert len(rows) == 120
	for digest, name in rows:
		pin(source / variant / name, digest)
	for flavor in ["o2", "san"]:
		pin(focused / ("build-" + variant + "-" + flavor) / "pointer-check")
output.mkdir(exist_ok=False)
(output / "temporary").mkdir()
save("inputs-before.json", pins)
environment = dict(os.environ, TMPDIR=str(output / "temporary"),
	ASAN_OPTIONS="detect_leaks=1:detect_stack_use_after_return=1:abort_on_error=1",
	UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
for case in cases:
	for flavor in ["o2", "san"]:
		for variant in ["parent", "candidate"]:
			label = case["case"] + "-" + variant + "-" + flavor
			argv = case["command"].copy()
			argv[0] = str(focused / ("build-" + variant + "-" + flavor) / "pointer-check")
			log = output / (label + ".log")
			censored = False
			with log.open("wb") as stream:
				process = subprocess.Popen(argv, cwd=output, env=environment,
					stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
				try:
					process.wait(timeout=60)
				except subprocess.TimeoutExpired:
					censored = True
					os.killpg(process.pid, signal.SIGKILL)
					process.wait()
			text = log.read_text()
			expected_word = "done" if case["expected_exit"] == 0 else "rejected"
			match = re.fullmatch(expected_word + r" steps=(\d+)\s*", text)
			row = {"case": case["case"], "variant": variant, "flavor": flavor, "argv": argv,
				"exit": process.returncode, "expected_exit": case["expected_exit"], "censored": censored,
				"output": text, "log_sha256": sha(log), "input_sha256": sha(Path(argv[-1])),
				"qualified": not censored and process.returncode == case["expected_exit"] and match is not None}
			if match:
				row["steps"] = int(match[1])
			records.append(row)
			save("records.json", records)
assert all(sha(Path(path)) == digest for path, digest in pins.items())
agreement = {}
for case in cases:
	group = [row for row in records if row["case"] == case["case"]]
	agreement[case["case"]] = all(row["qualified"] for row in group) and len({row["output"] for row in group}) == 1
save("inputs-after.json", pins)
summary = {"terminal": True, "source120": sources, "commands": len(records),
	"unexpected_failures": sum(not row["qualified"] for row in records),
	"parent_candidate_O2_SAN_output_and_fuel_agreement": agreement,
	"historical_endpoint_fuels": {row["case"]: row["steps"] for row in cases},
	"fresh_endpoint_fuels": {row["case"]: row.get("steps") for row in records if row["variant"] == "parent" and row["flavor"] == "o2"},
	"all_input_hashes_exact_after": True,
	"limits": ["Original hash-matched AP prefix fixtures and budget; fresh accepted94 parent/c206 candidate O2/SAN without a build or fixture/assertion/owner edit.",
		"Positive and false concrete endpoints only, not identical cross-system proof/helper work or a universal theorem.",
		"Any historical fuel change stays explicit; current parent/candidate output and fuel must agree.",
		"No wall/RSS collector, comparative grant, new runtime epoch, accepted/Main action or Goal completion.",
		"All original strict/observer/SAN/setup/censored/byte failures remain unwaived; broader accepted acceptance remains separate."]}
save("summary.json", summary)
print(json.dumps(summary, indent=2), flush=True)
assert len(records) == 12 and not summary["unexpected_failures"] and all(agreement.values())
