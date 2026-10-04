"""Retain strict public failures and compare both accepted-pair partition outcomes."""
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess


owner = Path(__file__).resolve().parent
focused = owner / "private/accepted94_order_stateless_frames_v1_focused_20261004"
completed = owner / "private/accepted94_order_stateless_frames_v1_completed_20261004"
output = owner / "private/accepted94_order_stateless_frames_v1_partitions_20261004"
script = owner.parent / "image_audit/partition_fuel.sh"
source = focused / "fixtures/examples/09_list_induction.p"
pins = json.loads((completed / "inputs-after.json").read_text())
records = []
rows = {}


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
	path.write_text(json.dumps(value, indent=2) + "\n")


assert all(sha(Path(name)) == digest for name, digest in pins.items())
for path in [Path(__file__), script, source, completed / "summary.json", completed / "inputs-after.json"]:
	pins[str(path)] = sha(path)
output.mkdir(exist_ok=False)
(output / "temporary").mkdir()
write_json(output / "inputs-before.json", pins)
environment = dict(os.environ, TMPDIR=str(output / "temporary"), IMAGE_AUDIT_STRICT_BYTES="1")
for variant in ["parent", "candidate"]:
	directory = output / ("partitions-" + variant)
	argv = ["bash", str(script), str(focused / ("build-" + variant + "-o2/pointer-check")),
		str(source), str(directory), "ordinary"]
	log = output / (variant + ".log")
	censored = False
	with log.open("wb") as stream:
		process = subprocess.Popen(argv, env=environment, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
		try:
			process.wait(timeout=360)
		except subprocess.TimeoutExpired:
			censored = True
			os.killpg(process.pid, signal.SIGKILL)
			process.wait()
	records.append({"label": variant, "argv": argv, "exit": process.returncode,
		"censored": censored, "log_sha256": sha(log)})
	write_json(output / "records.json", records)
	assert process.returncode == 1 and not censored, log
	lines = (directory / "partitions.tsv").read_text().splitlines()
	keys = lines[0].split("\t")
	rows[variant] = [dict(zip(keys, line.split("\t"))) for line in lines[1:]]
	assert len(rows[variant]) == 40
comparisons = [{"name": path.name, "parent_sha256": sha(path),
	"candidate_sha256": sha(output / "partitions-candidate" / path.name),
	"equal": path.read_bytes() == (output / "partitions-candidate" / path.name).read_bytes()}
	for path in sorted((output / "partitions-parent").glob("*.a"))]
assert len(comparisons) == 52 and all(row["equal"] for row in comparisons)
assert rows["parent"] == rows["candidate"]
failed = {variant: [row for row in table if any(row[key] != "yes"
	for key in ["size_equal", "byte_equal", "status_equal", "fuel_equal"])] for variant, table in rows.items()}
assert all(sha(Path(name)) == digest for name, digest in pins.items())
write_json(output / "inputs-after.json", pins)
write_json(output / "failed-comparison-rows.json", failed)
write_json(output / "public-image-comparisons.json", comparisons)
report = {"terminal": True, "sources": json.loads((focused / "summary.json").read_text())["sources"],
	"records": records, "strict_recipe_failures": 2, "strict_comparison_failures_unwaived": failed,
	"parent_candidate_full40_row_fuel_TSV_exact": True, "all52_images_equal_current_accepted_pair": True,
	"all_pins_exact_after": True, "all_launched_children_reaped": True,
	"profile": "Current accepted compact REPL save; historical materialized/default producer images are not substituted.",
	"historical_strict3_and_other_failures_retained": True,
	"no_byte_waiver_runtime_READY_cost_or_promotion": True}
write_json(output / "summary.json", report)
print(json.dumps({"terminal": True, "strict_failed_comparisons_each": {v: len(r) for v, r in failed.items()},
	"all52_images_equal": True, "summary_sha256": sha(output / "summary.json")}), flush=True)
