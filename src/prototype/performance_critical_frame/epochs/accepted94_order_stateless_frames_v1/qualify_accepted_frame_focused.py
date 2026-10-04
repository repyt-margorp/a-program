"""Qualify the accepted-baseline delta with unchanged committed and lifetime fixtures."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess


owner = Path(__file__).resolve().parent
worker = owner.parents[2]
prepared = owner / "private/accepted94_order_stateless_frames_v1_20261004"
clean = Path("/home/repyt/workspace/a-program-workers/merge-storage-20261003/accepted-job-promotion")
revision = "94a20003b976475b1ed38025e61acb6cdbdab92c"
output = owner / "private/accepted94_order_stateless_frames_v1_focused_20261004"
build_file = owner / "accepted_verify.mk"
fixtures = output / "fixtures"
frame_fixtures = output / "frame-fixtures"
sources = {"parent": "930997b62db85814718d0d5ac151f392bfae243a447d5d0d5f2adbde66cba5ae",
	"candidate": "c20635425ff3d587fe939f62970d3d6370941013fcb2e85ee0da5e10c4a08907"}
tests = ["frame_retirement_test", "stateless_head_cleanup_test", "callback_failure_cleanup_test",
	"fold_spine_test", "materialized_fields_test", "direct_test", "head_test", "total_result_test", "head_cleanup_test"]
records = []
pins = {}
origins = {}
outputs = {}


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
	path.write_text(json.dumps(value, indent=2) + "\n")


def copy(path, destination, origin):
	destination.parent.mkdir(parents=True, exist_ok=True)
	shutil.copyfile(path, destination)
	assert sha(path) == sha(destination)
	pins[str(path)] = sha(path)
	pins[str(destination)] = sha(destination)
	origins[str(destination)] = origin


def run(label, argv, timeout=600):
	argv = [str(value) for value in argv]
	log = output / (label + ".log")
	censored = False
	with log.open("wb") as stream:
		process = subprocess.Popen(argv, env=environment, cwd=fixtures,
			stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
		try:
			process.wait(timeout=timeout)
		except subprocess.TimeoutExpired:
			censored = True
			os.killpg(process.pid, signal.SIGKILL)
			process.wait()
	records.append({"label": label, "argv": argv, "exit": process.returncode,
		"censored": censored, "log_sha256": sha(log)})
	write_json(output / "records.json", records)
	print(label, "exit", process.returncode, "censored", censored, flush=True)
	return process.returncode == 0 and not censored


prepared_pins = json.loads((prepared / "inputs-after.json").read_text())
assert all(sha(Path(name)) == digest for name, digest in prepared_pins.items())
pins.update(prepared_pins)
for variant, digest in sources.items():
	root = prepared / variant
	assert sha(root / "source.sha256") == digest
	rows = [row.split("  ", 1) for row in (root / "source.sha256").read_text().splitlines()]
	assert len(rows) == 120
	for expected, name in rows:
		assert sha(root / name) == expected
		pins[str(root / name)] = expected
	pins[str(root / "source.sha256")] = digest
names = subprocess.check_output(["git", "ls-tree", "-r", "--name-only", revision, "--",
	"tests", "examples", "archive/legacy/src/prototype/tests/fixtures", "print.p", "training"],
	cwd=worker, text=True).splitlines()
expected = subprocess.check_output(["git", "rev-parse", *[revision + ":" + name for name in names]],
	cwd=worker, text=True).splitlines()
actual = subprocess.check_output(["git", "hash-object", *[str(clean / name) for name in names]],
	cwd=worker, text=True).splitlines()
assert len(names) == len(actual) == len(expected) and actual == expected
output.mkdir(exist_ok=False)
(output / "temporary").mkdir()
for name, blob in zip(names, actual):
	copy(clean / name, fixtures / name, {"source": str(clean / name), "revision": revision,
		"git_blob": blob, "sha256": sha(clean / name)})
spine = owner.parent / "performance_spine_current/private/current_spine_deletions_v1_20261004/fixtures"
resaver = Path("/home/repyt/workspace/a-program-workers/merge-storage-20261003/mem10-current-parent-inert-probe/machine_resave_probe.c")
frame_sources = {name: owner.parent / "performance_mem1" / (name + ".c") for name in tests[:3]}
frame_sources.update({name: spine / (name + ".c") for name in tests[3:5]})
frame_sources.update({"machine_resave_probe": resaver, "readback_transport_bounds_test": owner / "readback_transport_bounds_test.c"})
for name, path in frame_sources.items():
	if name in tests[:3]:
		assert path.read_bytes() == (owner.parent / "performance_mem1/epochs/head_frame_v3" / path.name).read_bytes()
	copy(path, frame_fixtures / (name + ".c"), {"source": str(path), "sha256": sha(path), "assertions_unchanged": True})
for path in [Path(__file__), build_file, prepared / "source-preparation.json", prepared / "inputs-after.json",
	prepared / "accepted94-frame-order.patch", Path("/usr/bin/x86_64-linux-gnu-gcc-14")]:
	pins[str(path)] = sha(path)
write_json(output / "fixture-origins.json", origins)
write_json(output / "inputs-before.json", pins)
environment = dict(os.environ, TMPDIR=str(output / "temporary"),
	ASAN_OPTIONS="detect_leaks=1:detect_stack_use_after_return=1:abort_on_error=1",
	UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
for flavor, flags in [("o2", "-std=c11 -Wall -Wextra -Werror -O2"),
	("san", "-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie")]:
	for variant in sources:
		root = prepared / variant
		build = output / ("build-" + variant + "-" + flavor)
		settings = ["OVERLAY=" + str(root), "ROOT=" + str(root / "src") + "/", "BUILD=" + str(build),
			"REPO=" + str(fixtures), "TESTS=" + str(fixtures / "tests"), "FRAME_FIXTURES=" + str(frame_fixtures),
			"CC=/usr/bin/x86_64-linux-gnu-gcc-14", "CFLAGS=" + flags]
		for name in [*tests, "machine_resave_probe", "pointer-check",
			*(["readback_transport_bounds_test"] if variant == "candidate" else [])]:
			binary = build / name
			if not run("build-" + variant + "-" + flavor + "-" + name,
				["make", "-j1", "-f", build_file, *settings, binary]):
				continue
			pins[str(binary)] = sha(binary)
			if name in ["machine_resave_probe", "pointer-check"]:
				continue
			label = "run-" + variant + "-" + flavor + "-" + name
			if run(label, [binary]):
				outputs[(variant, flavor, name)] = (output / (label + ".log")).read_bytes()
				if name == "total_result_test":
					run("fresh-TotalResult-" + variant + "-" + flavor,
						["python3", fixtures / "tests/total_result_cuts.py", binary,
							output / ("total-result-" + variant + "-" + flavor)])
		assert all(sha(Path(name)) == digest for name, digest in pins.items())
comparisons = [{"test": name, "flavor": flavor,
	"equal": ("parent", flavor, name) in outputs and ("candidate", flavor, name) in outputs
		and outputs[("parent", flavor, name)] == outputs[("candidate", flavor, name)]}
	for name in tests for flavor in ["o2", "san"]]
write_json(output / "same-flavor-output-comparisons.json", comparisons)
write_json(output / "inputs-after.json", pins)
summary = {"terminal": True, "sources": sources, "records": records,
	"failed": sum(row["exit"] != 0 or row["censored"] for row in records),
	"same_flavor_outputs": comparisons, "all18_outputs_equal": all(row["equal"] for row in comparisons),
	"fixture_git_revision": revision, "committed_fixture_count": len(names),
	"all_source_fixture_binary_pins_exact_after": True, "all_launched_children_reaped": True,
	"accepted_source_and_original_assertions_unchanged": True, "runtime_READY": False,
	"limits": ["Only four-file frame/order delta on accepted94; no excluded Surface/optimization/checkpoint runtime ingested.",
		"Bounds test's candidate-only order/count rejection is additive; original parent codec/lifetime controls are retained.",
		"Original historical/current bytes/strict/observer/SAN/setup/censored failures remain separately unwaived.",
		"Compact/materialized CLI persistence, affected consumers, fresh completed fuels and actual peak RSS/time remain pending; no cost grant/collector/promotion/Goal completion."]}
write_json(output / "summary.json", summary)
print(json.dumps({"terminal": True, "records": len(records), "failed": summary["failed"],
	"all18_outputs_equal": summary["all18_outputs_equal"], "summary_sha256": sha(output / "summary.json")}), flush=True)
