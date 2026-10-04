"""Pin fresh completed fuels and persistent public-profile bytes without cost measurements."""
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess


owner = Path(__file__).resolve().parent
focused = owner / "private/accepted94_order_stateless_frames_v1_focused_20261004"
consumers = owner / "private/accepted94_order_stateless_frames_v1_consumers_20261004"
output = owner / "private/accepted94_order_stateless_frames_v1_completed_20261004"
original_config = Path("/tmp/ap-performance-mem3-fold-cost-request-20261003/configuration-proposed.json")
records = []
pins = {}
fuels = {}
images = []
inert = []
resumes = []


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
	path.write_text(json.dumps(value, indent=2) + "\n")


def run(label, argv, expected=0):
	argv = [str(value) for value in argv]
	log = output / (label + ".log")
	censored = False
	with log.open("wb") as stream:
		process = subprocess.Popen(argv, env=environment, stdout=stream,
			stderr=subprocess.STDOUT, start_new_session=True)
		try:
			process.wait(timeout=360)
		except subprocess.TimeoutExpired:
			censored = True
			os.killpg(process.pid, signal.SIGKILL)
			process.wait()
	records.append({"label": label, "argv": argv, "exit": process.returncode,
		"expected_exit": expected, "censored": censored, "log_sha256": sha(log)})
	write_json(output / "records.json", records)
	assert process.returncode == expected and not censored, log
	print(label, "exit", process.returncode, "expected", expected, flush=True)
	return log.read_text()


summaries = {}
for name, root in [("focused", focused), ("consumers", consumers)]:
	summaries[name] = json.loads((root / "summary.json").read_text())
	assert summaries[name]["terminal"] and summaries[name]["failed"] == 0 and summaries[name]["all18_outputs_equal"]
	pins.update(json.loads((root / "inputs-after.json").read_text()))
	for file in ["summary.json", "inputs-after.json"]:
		pins[str(root / file)] = sha(root / file)
assert sha(original_config) == "4dab6b887e69aa60e2ab8f429f5ae503473414e25cdecf62c0fe79d3c98cf510"
configuration = json.loads(original_config.read_text())
jobs = [row for row in configuration["jobs"] if row["variant"] == "baseline" and row["repetition"] == 1]
assert len(jobs) == 6
for row in jobs:
	for argument in row["argv"][1:]:
		path = Path(argument)
		if path.is_file():
			assert sha(path) == configuration["input_sha256"][str(path)]
			pins[str(path)] = sha(path)
for path in [Path(__file__), original_config]:
	pins[str(path)] = sha(path)
assert all(sha(Path(name)) == digest for name, digest in pins.items())
output.mkdir(exist_ok=False)
(output / "temporary").mkdir()
environment = dict(os.environ, TMPDIR=str(output / "temporary"),
	ASAN_OPTIONS="detect_leaks=1:detect_stack_use_after_return=1:abort_on_error=1",
	UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
write_json(output / "inputs-before.json", pins)
write_json(output / "six-original-source-controls.json", jobs)
for variant in ["parent", "candidate"]:
	for row in jobs:
		binary = focused / ("build-" + variant + "-o2/pointer-check")
		text = run("completed-" + variant + "-" + row["case"], [binary, *row["argv"][1:]])
		match = re.fullmatch(r"done steps=(\d+)\s*", text)
		assert match, text
		steps = int(match[1])
		if variant == "parent":
			fuels[row["case"]] = steps
		else:
			assert steps == fuels[row["case"]], (row["case"], steps, fuels[row["case"]])
for row in jobs:
	if row["case"] in ["list", "imported-local-sorted"]:
		text = run("completed-candidate-san-" + row["case"],
			[focused / "build-candidate-san/pointer-check", *row["argv"][1:]])
		assert text.strip() == "done steps=" + str(fuels[row["case"]])
list_input = Path(next(row for row in jobs if row["case"] == "list")["argv"][-1])
cuts = sorted({0, 1, 100, fuels["list"] // 2, fuels["list"] - 1, fuels["list"]})
for profile in ["--save", "--save-materialized"]:
	for cut in cuts:
		written = {}
		for variant in ["parent", "candidate"]:
			binary = focused / ("build-" + variant + "-o2/pointer-check")
			label = profile[2:] + "-" + variant + "-" + str(cut)
			image = output / (label + ".a")
			text = run("write-" + label, [binary, "--steps", str(cut), profile, image, list_input],
				expected=0 if cut == fuels["list"] else 3)
			assert text.strip() == ("done" if cut == fuels["list"] else "pending") + " steps=" + str(cut)
			written[variant] = image
			for reader in ["parent", "candidate"]:
				for flavor in ["o2", "san"]:
					binary = focused / ("build-" + reader + "-" + flavor + "/pointer-check")
					resaved = output / (label + "-inert-" + reader + "-" + flavor + ".a")
					text = run("inert-" + reader + "-" + flavor + "-" + label,
						[binary, "--load", "--steps", "0", profile, resaved, image], expected=3)
					assert text.strip() == "pending steps=0"
					inert.append({"profile": profile, "cut": cut, "writer": variant, "reader": reader,
						"flavor": flavor, "original_sha256": sha(image), "inert_sha256": sha(resaved),
						"equal": image.read_bytes() == resaved.read_bytes()})
					text = run("resume-" + reader + "-" + flavor + "-" + label, [binary, "--load", image])
					assert re.fullmatch(r"done steps=\d+\s*", text)
					resumes.append({"profile": profile, "cut": cut, "writer": variant, "reader": reader,
						"flavor": flavor, "output": text.strip()})
		images.append({"profile": profile, "cut": cut, "parent_sha256": sha(written["parent"]),
			"candidate_sha256": sha(written["candidate"]),
			"equal": written["parent"].read_bytes() == written["candidate"].read_bytes()})
		write_json(output / "public-profile-image-pairs.json", images)
		write_json(output / "public-profile-inert-rows.json", inert)
		write_json(output / "public-profile-resume-rows.json", resumes)
for profile in ["--save", "--save-materialized"]:
	for cut in cuts:
		rows = [row for row in resumes if row["profile"] == profile and row["cut"] == cut]
		assert len(rows) == 8 and len({row["output"] for row in rows}) == 1, rows
assert all(sha(Path(name)) == digest for name, digest in pins.items())
write_json(output / "inputs-after.json", pins)
report = {"terminal": True, "sources": summaries["focused"]["sources"], "records": records,
	"unexpected_command_failures": 0, "six_fresh_accepted_completed_fuels": fuels,
	"historical_E25_expected_fuels_retained": {row["case"]: row["expected_charged_steps"] for row in jobs},
	"all_parent_candidate_completed_fuels_equal": True, "public_profile_image_pairs": images,
	"paired_byte_failures_unwaived": sum(not row["equal"] for row in images),
	"inert_rows": len(inert), "inert_byte_failures_unwaived": sum(not row["equal"] for row in inert),
	"all96_public_profile_resume_verdict_fuels_equal_between_readers_and_writers": True,
	"all_source_tool_fixture_binary_pins_exact_after": True, "all_launched_children_reaped": True,
	"no_time_RSS_metrics_or_collector": True, "runtime_READY": False,
	"limits": ["Same six complete AP proof inputs/argv suffixes as original cost request; independently observed accepted fuels, no fuel-policy/assertion edits.",
		"Compact default and explicit materialized images preserved at six list prefixes, four O2/SAN inert/read consumers each; every byte difference remains failed.",
		"Public saves preserve recomputable inputs/available typed results, not full machine checkpoints. Raw machine splits are separately tested.",
		"Original strict3 and all historical byte/observer/SAN/setup/censored failures remain unwaived; no timing grant, promotion or full Goal completion."]}
write_json(output / "summary.json", report)
print(json.dumps({"terminal": True, "records": len(records), "fuels": fuels,
	"paired_bytes_failed": report["paired_byte_failures_unwaived"], "inert_bytes_failed": report["inert_byte_failures_unwaived"],
	"summary_sha256": sha(output / "summary.json")}), flush=True)
