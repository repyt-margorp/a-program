"""Check each accepted-baseline continuation split in fresh unchanged consumers."""
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess


owner = Path(__file__).resolve().parent
focused = owner / "private/accepted94_order_stateless_frames_v1_focused_20261004"
output = owner / "private/accepted94_order_stateless_frames_v1_cuts_20261004"
sources = {"parent": "930997b62db85814718d0d5ac151f392bfae243a447d5d0d5f2adbde66cba5ae",
	"candidate": "c20635425ff3d587fe939f62970d3d6370941013fcb2e85ee0da5e10c4a08907"}
counts = {}
totals = {}
records = []
pairs = []
inert = []
pins = {}
binaries = {}


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
	path.write_text(json.dumps(value, indent=2) + "\n")


def run(label, argv):
	argv = [str(value) for value in argv]
	log = output / (label + ".log")
	censored = False
	with log.open("wb") as stream:
		process = subprocess.Popen(argv, env=environment, stdout=stream,
			stderr=subprocess.STDOUT, start_new_session=True)
		try:
			process.wait(timeout=180)
		except subprocess.TimeoutExpired:
			censored = True
			os.killpg(process.pid, signal.SIGKILL)
			process.wait()
	records.append({"label": label, "argv": argv, "exit": process.returncode,
		"censored": censored, "log_sha256": sha(log)})
	write_json(output / "records.json", records)
	assert process.returncode == 0 and not censored, log
	return log.read_text()


summary = json.loads((focused / "summary.json").read_text())
assert summary["terminal"] and summary["failed"] == 0 and summary["all18_outputs_equal"]
for test, pattern, expected_cases in [
	("fold_spine_test", r"Fold case=(\d+) steps=(\d+) cuts=(\d+) pass", 5),
	("materialized_fields_test", r"Materialized case=(\d+) base=\d+ steps=(\d+) cuts=(\d+) pass", 7)]:
	text = (focused / ("run-parent-o2-" + test + ".log")).read_text()
	rows = [tuple(map(int, row)) for row in re.findall(pattern, text)]
	assert [row[0] for row in rows] == list(range(expected_cases))
	counts[test] = [row[2] for row in rows]
	totals[test] = [row[1] for row in rows]
for variant, directory in [("parent", focused), ("candidate", focused)]:
	summary = json.loads((directory / "summary.json").read_text())
	assert summary["terminal"] and summary["failed"] == 0
	assert summary["sources"][variant] == sources[variant]
	old_pins = json.loads((directory / "inputs-after.json").read_text())
	assert all(sha(Path(name)) == digest for name, digest in old_pins.items())
	pins.update(old_pins)
	for path in [directory / "summary.json", directory / "inputs-after.json"]:
		pins[str(path)] = sha(path)
	for flavor in ["o2", "san"]:
		build = directory / ("build-" + variant + "-" + flavor)
		for test in [*counts, "machine_resave_probe"]:
			binary = build / test
			assert sha(binary) == old_pins[str(binary)]
			binaries[(test, variant, flavor)] = binary
pins[str(Path(__file__))] = sha(Path(__file__))
output.mkdir(exist_ok=False)
(output / "temporary").mkdir()
environment = dict(os.environ, TMPDIR=str(output / "temporary"),
	ASAN_OPTIONS="detect_leaks=1:detect_stack_use_after_return=1:abort_on_error=1",
	UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
write_json(output / "inputs-before.json", pins)
write_json(output / "independent-parent-unit-cut-ranges.json", {"counts": counts, "totals": totals,
	"origin": "Unchanged accepted-parent units; all candidate same-flavor outputs equal before separate-process splits.",
	"old_E25_ranges_unchanged_historical": {"fold": [12, 16, 13, 14, 12], "materialized": [20, 20, 7, 5, 7, 11, 9]}})
for test, cases in counts.items():
	for case, count in enumerate(cases):
		for cut in range(count):
			images = {}
			for writer in sources:
				label = test + "-" + writer + "-" + str(case) + "-" + str(cut)
				image = output / (label + ".image")
				run("write-" + label, [binaries[(test, writer, "o2")], "write", image, str(cut), str(case)])
				images[writer] = image
				for flavor in ["o2", "san"]:
					for reader in sources:
						text = run("read-" + reader + "-" + flavor + "-" + label,
							[binaries[(test, reader, flavor)], "read", image])
						assert "total=" + str(totals[test][case]) + " pass" in text, text
					resaved = output / (label + "-inert-" + flavor + ".image")
					run("inert-" + flavor + "-" + label,
						[binaries[("machine_resave_probe", writer, flavor)], image, resaved])
					inert.append({"test": test, "case": case, "cut": cut, "writer": writer,
						"resaver_flavor": flavor, "writer_sha256": sha(image), "resaved_sha256": sha(resaved),
						"equal": image.read_bytes() == resaved.read_bytes()})
			left, right = images["parent"].read_bytes(), images["candidate"].read_bytes()
			pairs.append({"test": test, "case": case, "cut": cut,
				"parent_sha256": sha(images["parent"]), "candidate_sha256": sha(images["candidate"]),
				"equal": left == right, "parent_bytes": len(left), "candidate_bytes": len(right),
				"different_offsets": [i for i in range(min(len(left), len(right))) if left[i] != right[i]]})
			write_json(output / "byte-pairs.json", pairs)
			write_json(output / "inert-byte-rows.json", inert)
		print(test, "case", case, "cuts", count, "four fresh readers and O2/SAN inert resaves terminal", flush=True)
number = sum(sum(row) for row in counts.values())
assert len(records) == 14 * number and len(pairs) == number and len(inert) == 4 * number
assert all(sha(Path(name)) == digest for name, digest in pins.items())
write_json(output / "inputs-after.json", pins)
summary = {"terminal": True, "sources": sources, "records": len(records),
	"semantic_fuel_zero_budget_SAN_command_failures": 0, "paired_images": len(pairs),
	"paired_byte_failures_unwaived": sum(not row["equal"] for row in pairs),
	"inert_images": len(inert), "inert_byte_failures_unwaived": sum(not row["equal"] for row in inert),
	"Fold_cuts": sum(counts["fold_spine_test"]), "materialized_cuts": sum(counts["materialized_fields_test"]),
	"fresh_O2_reads": 4 * number, "fresh_SAN_reads": 4 * number,
	"candidate_inert_byte_failures_unwaived": sum(not row["equal"] for row in inert if row["writer"] == "candidate"),
	"parent_inert_byte_failures_unwaived": sum(not row["equal"] for row in inert if row["writer"] == "parent"),
	"all_current_sources_fixtures_binaries_reused_records_pins_exact_after": True,
	"all_launched_children_reaped": True, "runtime_READY": False,
	"limits": ["Exact accepted94 parent versus four-file frame/order delta; no new build, excluded module or fixture/assertion edit.",
		"Every raw paired/inert byte difference remains a failure; every historical failed artifact remains retained and unwaived.",
		"Cut ranges independently observed on unchanged accepted parent; old E25 materialized fuel/ranges remain historical, not current assertions.",
		"Compact/materialized CLI profile, affected current consumers and completed-task fuels remain separate.",
		"No actual matched peak RSS/time, cost grant/collector, accepted promotion or Goal completion."]}
write_json(output / "summary.json", summary)
print(json.dumps({**summary, "summary_sha256": sha(output / "summary.json")}), flush=True)
