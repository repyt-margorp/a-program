"""Pin a normal-O2 matched current pair; do not launch any cost workload."""
import copy
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess


owner = Path(__file__).resolve().parent
epoch = owner / "epochs/accepted94_order_stateless_frames_v1"
prepared = owner / "private/accepted94_order_stateless_frames_v1_20261004"
focused = owner / "private/accepted94_order_stateless_frames_v1_focused_20261004"
completed = owner / "private/accepted94_order_stateless_frames_v1_completed_20261004"
output = owner / "private/accepted94_order_stateless_frames_v1_cost_proposal_20261004"
published = owner / "epochs/accepted94_order_stateless_frames_v1_cost_proposal"
original_path = Path("/tmp/ap-performance-mem3-fold-cost-request-20261003/configuration-proposed.json")
collector = Path("/tmp/ap-performance-measure-gnu-time-e6-20261003.py")
tools = [collector, Path("/tmp/a-program-merge-timing-tool-20261003/extracted/usr/bin/time"),
	Path("/tmp/a-program-merge-timing-tool-20261003/time_1.9-0.2_amd64.deb"),
	Path("/lib/x86_64-linux-gnu/libc.so.6"), Path("/lib64/ld-linux-x86-64.so.2"), Path("/usr/bin/timeout")]
variants = {"baseline": "parent", "critical-frames-and-order": "candidate"}
pins = {}
provenance = {}


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def pin(path, expected=None):
	digest = sha(path)
	assert expected is None or digest == expected, path
	pins[str(path)] = digest
	return digest


def write_json(path, value):
	path.write_text(json.dumps(value, indent=2) + "\n")


pin(original_path, "4dab6b887e69aa60e2ab8f429f5ae503473414e25cdecf62c0fe79d3c98cf510")
original = json.loads(original_path.read_text())
for path in tools:
	pin(path, original["input_sha256"][str(path)])
pin(collector, "9c50c62d1ddc49e85c88abe94f95162c85096165afcdff7cd945bb9b9a004b71")
pin(epoch / "manifest.sha256", "62491132e5cf717662802b53296d15fc5a50b39d7e27011079fcaacef3c43d2b")
for row in (epoch / "manifest.sha256").read_text().splitlines():
	digest, name = row.split("  ", 1)
	assert sha(epoch / name) == digest
handoff = json.loads((epoch / "handoff.json").read_text())
assert handoff["all_children_stopped"] and handoff["raw_semantic_fuel_SAN2296_failed"] == 0
for row in handoff["private_evidence"].values():
	root = Path(row["path"])
	pin(root / "verification.sha256", row["manifest_sha256"])
	for line in (root / "verification.sha256").read_text().splitlines():
		digest, name = line.split("  ", 1)
		assert sha(root / name) == digest
	for name in ["summary.json", "inputs-after.json"]:
		if (root / name).is_file(): pin(root / name)
proof = json.loads((completed / "summary.json").read_text())
assert proof["terminal"] and proof["unexpected_command_failures"] == 0 and proof["all_parent_candidate_completed_fuels_equal"]
source_rows = {}
binaries = {}
for variant, directory in variants.items():
	root = prepared / directory
	source_rows[variant] = {name: digest for digest, name in
		(row.split("  ", 1) for row in (root / "source.sha256").read_text().splitlines())}
	assert len(source_rows[variant]) == 120
	pin(root / "source.sha256", proof["sources"][directory])
	for name, digest in source_rows[variant].items(): pin(root / name, digest)
	for name in ["Makefile", "src/Makefile"]: pin(root / name)
	binaries[variant] = focused / ("build-" + directory + "-o2/pointer-check")
	pin(binaries[variant])
	log = focused / ("build-" + directory + "-o2-pointer-check.log")
	pin(log)
	lines = [line for line in log.read_text().splitlines() if " -o " in line
		and line.split(" -o ")[-1] == str(binaries[variant])]
	assert len(lines) == 1
	argv = shlex.split(lines[0])
	assert argv[1:6] == ["-std=c11", "-Wall", "-Wextra", "-Werror", "-O2"]
	assert "-g" not in argv and not any("--wrap" in argument or "sanitize" in argument for argument in argv)
	compiled = [Path(argument) for argument in argv if argument.endswith(".c")]
	assert compiled and all(str(path.relative_to(root)) in source_rows[variant] for path in compiled)
	compiler = Path(shutil.which(argv[0])).resolve()
	pin(compiler)
	nm = subprocess.run(["/usr/bin/nm", binaries[variant]], capture_output=True, text=True, check=True)
	assert not any(symbol in nm.stdout for symbol in ["__wrap_pg_alloc", "__asan_", "__ubsan_"])
	provenance[variant] = {"binary": str(binaries[variant]), "binary_sha256": sha(binaries[variant]),
		"compiler_command": argv, "compiler_actual": str(compiler), "compiled_sources": len(compiled),
		"source120": sha(root / "source.sha256"), "normal_O2_no_wrapper_SAN_debug": True}
pin(Path("/usr/bin/nm"))
pin(Path(__file__))
pin(epoch / "handoff.json")
changed = sorted(name for name in source_rows["baseline"]
	if source_rows["baseline"][name] != source_rows["critical-frames-and-order"][name])
assert changed == ["src/eval.c", "src/eval.h", "src/eval_internal.h", "src/eval_io.c"]
jobs = copy.deepcopy(original["jobs"])
assert len(jobs) == 36
for row, previous in zip(jobs, original["jobs"]):
	variant = "baseline" if row["variant"] == "baseline" else "critical-frames-and-order"
	row["variant"] = variant
	row["argv"][0] = str(binaries[variant])
	row["expected_charged_steps"] = proof["six_fresh_accepted_completed_fuels"][row["case"]]
	assert row["argv"][1:] == previous["argv"][1:] and row["system"] == "ap"
	assert row["repetition"] == previous["repetition"]
	for argument in row["argv"][1:]:
		path = Path(argument)
		if path.is_file(): pin(path, original["input_sha256"][str(path)])
assert {row["repetition"] for row in jobs} == {1, 2, 3} and len({row["case"] for row in jobs}) == 6
output.mkdir(exist_ok=False)
write_json(output / "build-provenance.json", provenance)
write_json(output / "correctness.json", {"source_epoch_manifest": sha(epoch / "manifest.sha256"),
	"focused88_consumers80_raw_semantic2296_completed230_terminal": True,
	"public_profile_pairs12_inert96_resume96_exact": True,
	"strict3_each_current_pair_retained_full40_TSV_and52_images_exact": True,
	"raw_parent36_inert_and20_paired_byte_failures_unwaived_candidate_inert328_exact": True,
	"fresh_completed_fuels": proof["six_fresh_accepted_completed_fuels"],
	"historical_LocalSorted761848_is_now956507_on_accepted_parent_and_candidate": True,
	"broader_acceptance_downstream_C_and_original_failures_remain_separate": True,
	"all_original_proposals_freezes_unchanged_no_actual_RSS_time_claim": True})
for name in ["build-provenance.json", "correctness.json"]: pin(output / name)
assert all(sha(Path(name)) == digest for name, digest in pins.items())
configuration = {"exclusive_slot_required": True, "maximum_seconds": 600, "per_run_seconds": 180,
	"canonical_producer": "Committed accepted94a20003 runtime120 versus only four-file critical frame/order delta; no Surface or excluded prototype optimization/checkpoint modules",
	"baseline_source120": proof["sources"]["parent"], "candidate_source120": proof["sources"]["candidate"],
	"only_different_runtime_files": changed, "CFLAGS": "-std=c11 -Wall -Wextra -Werror -O2",
	"input_sha256": pins, "jobs": jobs, "source_epoch_manifest": sha(epoch / "manifest.sha256"),
	"no_workload_launched_by_this_helper": True, "no_native_build_extra_workload": True,
	"grant_launch_rule": "New explicit exclusive grant only: derive ONLY maximum_seconds=min(600,floor(remaining absolute seconds)-2), record UTC/remaining/original/derived hashes; stop all children before hard deadline.",
	"limits": original["limits"][:5] + [
		"Fresh sequential completed AP processes, three repetitions; matching current source/proof/helper/input/fuel work. No historical producer or universal/native ranking transfer.",
		"Accepted compact save profile differs from old prototype; these jobs do not save. LocalSorted expected956507 is independently observed, original761848 remains historical.",
		"Net full candidate includes ordinal metadata: entry88->96/single aligned96 same; loader and writer each8*n before rounding. Nine current primary extents unchanged, no binary ABI claim.",
		"Peak GNU ru_maxrss is process-wide, includes startup/library/parser/proof work; launcher elapsed includes timeout/tool launch. Small-list GNU wall resolution/startup limits apply.",
		"Original strict3, raw parent36/paired20 byte failures and all earlier observer/SAN7/V1/V2/setup/compatibility/censored failures remain unwaived. Broader acceptance/downstream C remain separate.",
		"No cost sample/grant, accepted promotion or full Goal completion inferred from this request."]}
write_json(output / "configuration-proposed.json", configuration)
write_json(output / "proposal.json", {"lane": "performance", "issue": "#56 / PR #58",
	"status": "REQUEST only, ungranted/unlaunched", "requested_seconds": 600, "jobs": 36, "repetitions": 3,
	"all_pins": len(pins), "configuration_sha256": sha(output / "configuration-proposed.json"),
	"collector_sha256": sha(collector), "source_epoch_manifest": sha(epoch / "manifest.sha256"),
	"branch": "parallel/performance-20261003", "task_parent": "e71528fa286e263c568d4f78f3f1281f90b5e86d",
	"no_owned_heavy_children_at_preparation_or_worker_collector": True})
published.mkdir(exist_ok=False)
for name in ["configuration-proposed.json", "proposal.json", "correctness.json", "build-provenance.json"]:
	shutil.copyfile(output / name, published / name)
	assert sha(output / name) == sha(published / name)
shutil.copyfile(Path(__file__), published / Path(__file__).name)
files = sorted(path for path in published.rglob("*") if path.is_file())
manifest = published / "manifest.sha256"
manifest.write_text("".join(sha(path) + "  " + str(path.relative_to(published)) + "\n" for path in files))
assert all(sha(Path(name)) == digest for name, digest in pins.items())
print(json.dumps({"manifest": str(manifest), "manifest_sha256": sha(manifest), "files_including_manifest": len(files) + 1,
	"configuration": str(output / "configuration-proposed.json"), "configuration_sha256": sha(output / "configuration-proposed.json"),
	"pins": len(pins), "jobs": 36, "fresh_fuels": proof["six_fresh_accepted_completed_fuels"], "ungranted_unlaunched": True}), flush=True)
