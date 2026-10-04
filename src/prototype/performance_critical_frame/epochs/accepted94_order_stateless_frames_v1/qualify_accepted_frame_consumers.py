"""Run accepted affected consumers and both public save profiles without adapters."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess


owner = Path(__file__).resolve().parent
prepared = owner / "private/accepted94_order_stateless_frames_v1_20261004"
focused = owner / "private/accepted94_order_stateless_frames_v1_focused_20261004"
fixtures = focused / "fixtures"
output = owner / "private/accepted94_order_stateless_frames_v1_consumers_20261004"
build_file = owner / "accepted_verify.mk"
tests = ["core_test", "iadt_test", "synthesis_test", "eval_io_test", "source_io_test", "identity_io_test",
	"host_test", "execution_test", "identity_test"]
records = []
pins = {}
outputs = {}


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
	path.write_text(json.dumps(value, indent=2) + "\n")


def run(label, argv, timeout=600):
	argv = [str(value) for value in argv]
	log = output / (label + ".log")
	censored = False
	with log.open("wb") as stream:
		process = subprocess.Popen(argv, cwd=fixtures, env=environment,
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


summary = json.loads((focused / "summary.json").read_text())
assert summary["terminal"] and summary["failed"] == 0 and summary["all18_outputs_equal"]
pins.update(json.loads((focused / "inputs-after.json").read_text()))
assert all(sha(Path(name)) == digest for name, digest in pins.items())
for path in [Path(__file__), build_file, focused / "summary.json", focused / "inputs-after.json"]:
	pins[str(path)] = sha(path)
output.mkdir(exist_ok=False)
(output / "temporary").mkdir()
write_json(output / "inputs-before.json", pins)
environment = dict(os.environ, TMPDIR=str(output / "temporary"),
	ASAN_OPTIONS="detect_leaks=1:detect_stack_use_after_return=1:abort_on_error=1",
	UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
for flavor, flags in [("o2", "-std=c11 -Wall -Wextra -Werror -O2"),
	("san", "-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie")]:
	for variant in summary["sources"]:
		root = prepared / variant
		build = output / ("build-" + variant + "-" + flavor)
		build.mkdir()
		old_cli = focused / ("build-" + variant + "-" + flavor) / "pointer-check"
		shutil.copy2(old_cli, build / "pointer-check")
		assert sha(old_cli) == sha(build / "pointer-check")
		pins[str(build / "pointer-check")] = sha(old_cli)
		settings = ["OVERLAY=" + str(root), "ROOT=" + str(root / "src") + "/", "BUILD=" + str(build),
			"REPO=" + str(fixtures), "TESTS=" + str(fixtures / "tests"),
			"CC=/usr/bin/x86_64-linux-gnu-gcc-14", "CFLAGS=" + flags]
		passed = []
		for name in [*tests, "program_test"]:
			binary = build / name
			if not run("build-" + variant + "-" + flavor + "-" + name,
				["make", "-j1", "-f", build_file, *settings, binary]):
				continue
			pins[str(binary)] = sha(binary)
			passed.append(name)
			if name == "program_test":
				continue
			argv = [binary]
			if name in ["eval_io_test", "source_io_test", "identity_io_test"]:
				argv = ["bash", fixtures / "tests" / (name.removesuffix("_test") + ".sh"), binary]
			label = "run-" + variant + "-" + flavor + "-" + name
			if run(label, argv):
				outputs[(variant, flavor, name)] = sha(output / (label + ".log"))
		if "source_io_test" in passed and "program_test" in passed:
			run("image-cli-profiles-" + variant + "-" + flavor,
				["bash", fixtures / "tests/image_cli.sh", build / "source_io_test",
					build / "pointer-check", build / "program_test"])
		assert all(sha(Path(name)) == digest for name, digest in pins.items())
comparisons = [{"test": name, "flavor": flavor,
	"equal": ("parent", flavor, name) in outputs and ("candidate", flavor, name) in outputs
		and outputs[("parent", flavor, name)] == outputs[("candidate", flavor, name)]}
	for name in tests for flavor in ["o2", "san"]]
write_json(output / "same-flavor-consumer-outputs.json", comparisons)
write_json(output / "inputs-after.json", pins)
report = {"terminal": True, "sources": summary["sources"], "records": records,
	"failed": sum(row["exit"] != 0 or row["censored"] for row in records),
	"same_flavor_outputs": comparisons, "all18_outputs_equal": all(row["equal"] for row in comparisons),
	"all_source_fixture_tool_binary_pins_exact_after": True, "all_launched_children_reaped": True,
	"runtime_READY": False,
	"limits": ["Exact accepted94 committed tests and source; no adapter/assertion/owner or accepted edits.",
		"Core/IADT/Synthesis/eval-IO/full SourceIO/IdentityIO/host/execution/identity O2 and sanitizer consumers.",
		"Original image_cli.sh covers compact default/alias/REPL, explicit materialized inert byte checks, negative/partial images and resume readback; generated shell temporary images follow the original cleanup policy.",
		"Excluded prototype checkpoint modules are not imported into accepted qualification; old seven-checkpoint evidence remains separately attributed.",
		"Every historical failure remains unwaived; preserved raw byte-pair/cuts and completed-task fuels are separate.",
		"No actual RSS/time collector, cost grant, runtime publication, accepted promotion or Goal completion."]}
write_json(output / "summary.json", report)
print(json.dumps({"terminal": True, "records": len(records), "failed": report["failed"],
	"all18_outputs_equal": report["all18_outputs_equal"], "summary_sha256": sha(output / "summary.json")}), flush=True)
