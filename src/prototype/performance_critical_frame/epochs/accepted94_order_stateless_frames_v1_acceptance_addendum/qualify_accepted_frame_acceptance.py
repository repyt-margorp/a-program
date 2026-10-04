"""Run the original accepted acceptance gate and retain every recipe exit."""
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
from datetime import datetime, timezone


owner = Path(__file__).resolve().parent
prepared = owner / "private/accepted94_order_stateless_frames_v1_acceptance_20261004"
trial = prepared / "trial"
output = owner / "private/accepted94_order_stateless_frames_v1_full_acceptance_20261004"
shell = owner.parent / "performance_verification/make_shell.py"
timeout_seconds = 7200


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
	path.write_text(json.dumps(value, indent=2) + "\n")


preparation = json.loads((prepared / "preparation.json").read_text())
assert preparation["make_dry_run_exit"] == 0
assert preparation["source120"] == "c20635425ff3d587fe939f62970d3d6370941013fcb2e85ee0da5e10c4a08907"
pins = json.loads((prepared / "inputs-before.json").read_text())
assert all(sha(Path(name)) == digest for name, digest in pins.items())
for path in [Path(__file__), prepared / "preparation.json", prepared / "inputs-before.json", shell]:
	pins[str(path)] = sha(path)
output.mkdir(exist_ok=False)
(output / "temporary").mkdir()
write_json(output / "inputs-before.json", pins)
log = output / "full-acceptance.log"
recipes_path = output / "recipes.jsonl"
argv = ["make", "-j1", "-k", "-i", "-f", str(trial / "src/Makefile"),
	"CC=/usr/bin/x86_64-linux-gnu-gcc-14", "CFLAGS=-std=c11 -Wall -Wextra -Werror -O2",
	"BUILD=" + str(prepared / "build-candidate-o2"), "SHELL=" + str(shell), "check-acceptance"]
environment = dict(os.environ, TMPDIR=str(output / "temporary"), PERFORMANCE_GATE_LOG=str(recipes_path))
start = datetime.now(timezone.utc).isoformat()
censored = False
with log.open("wb") as stream:
	process = subprocess.Popen(argv, cwd=trial, env=environment,
		stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
	write_json(output / "launched-process.json", {"pid": process.pid, "process_group": process.pid,
		"argv": argv, "cwd": str(trial), "start_UTC": start, "timeout_seconds": timeout_seconds})
	try:
		process.wait(timeout=timeout_seconds)
	except subprocess.TimeoutExpired:
		censored = True
		os.killpg(process.pid, signal.SIGKILL)
		process.wait()
recipes = [json.loads(line) for line in recipes_path.read_text().splitlines()] if recipes_path.exists() else []
failed = [row for row in recipes if row["exit"] != 0]
write_json(output / "failed-recipes.json", failed)
assert all(sha(Path(name)) == digest for name, digest in pins.items())
for path in (prepared / "build-candidate-o2").glob("*"):
	if path.is_file():
		pins[str(path)] = sha(path)
write_json(output / "inputs-after.json", pins)
record = {"argv": argv, "cwd": str(trial), "exit": process.returncode, "censored": censored,
	"log_sha256": sha(log), "start_UTC": start, "terminal_UTC": datetime.now(timezone.utc).isoformat()}
write_json(output / "record.json", record)
summary = {"terminal": True, "source120": preparation["source120"], "record": record,
	"recorded_recipes": len(recipes), "failed_recipes": len(failed), "failed_recipe_records": failed,
	"passed": bool(recipes) and not failed and not censored and process.returncode == 0,
	"all_pins_exact_after": True, "all_launched_children_reaped": True,
	"old_E25_1200s_incomplete_run_and_all_original_failures_retained": True,
	"limits": ["Original accepted94 check-acceptance on exact candidate c206; per-recipe exits authoritative, aggregate Make zero alone is not acceptance.",
		"No fixture/assertion/adapter/owner/runtime edits; existing preparation seeds byte-exact current O2 binaries in mutable private output.",
		"Disk-backed output/TMPDIR, correctness-j1 only; coordinate drain before an exclusive benchmark slot.",
		"New failed recipes require unchanged current-parent controls, not an automatic baseline waiver.",
		"No timing/RSS collector, accepted edit/promotion/Main action or Goal completion."]}
write_json(output / "summary.json", summary)
print(json.dumps({"terminal": True, "records": len(recipes), "failed": len(failed), "passed": summary["passed"],
	"censored": censored, "summary_sha256": sha(output / "summary.json")}), flush=True)
