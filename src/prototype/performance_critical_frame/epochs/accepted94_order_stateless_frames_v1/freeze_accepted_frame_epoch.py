"""Freeze the current accepted-pair source and terminal evidence without waiving bytes."""
import hashlib
import json
from pathlib import Path
import shutil


owner = Path(__file__).resolve().parent
epoch = owner / "epochs/accepted94_order_stateless_frames_v1"
prepared = owner / "private/accepted94_order_stateless_frames_v1_20261004"
directories = {name: owner / ("private/accepted94_order_stateless_frames_v1_" + name + "_20261004")
	for name in ["focused", "cuts", "consumers", "completed", "partitions", "layouts", "layouts_retry"]}
directories["source"] = prepared
directories["historical_E25_censored_acceptance"] = owner / "private/current_e25_order_stateless_frames_v1_full_acceptance_20261004"


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def copy(path, name):
	destination = epoch / name
	destination.parent.mkdir(parents=True, exist_ok=True)
	shutil.copyfile(path, destination)
	assert sha(path) == sha(destination)


summaries = {name: json.loads((directory / "summary.json").read_text())
	for name, directory in directories.items() if name not in ["source", "layouts"]}
for name in ["focused", "consumers"]:
	assert summaries[name]["terminal"] and summaries[name]["failed"] == 0 and summaries[name]["all18_outputs_equal"]
assert summaries["cuts"]["records"] == 2296 and summaries["cuts"]["semantic_fuel_zero_budget_SAN_command_failures"] == 0
assert summaries["cuts"]["candidate_inert_byte_failures_unwaived"] == 0
assert summaries["cuts"]["parent_inert_byte_failures_unwaived"] == 36
assert summaries["cuts"]["paired_byte_failures_unwaived"] == 20
assert summaries["completed"]["terminal"] and summaries["completed"]["unexpected_command_failures"] == 0
assert summaries["completed"]["paired_byte_failures_unwaived"] == summaries["completed"]["inert_byte_failures_unwaived"] == 0
assert summaries["partitions"]["parent_candidate_full40_row_fuel_TSV_exact"]
assert all(len(rows) == 3 for rows in summaries["partitions"]["strict_comparison_failures_unwaived"].values())
assert summaries["layouts_retry"]["all_nine_sizes_alignments_extents_and_other_field_maps_equal"]
assert summaries["historical_E25_censored_acceptance"]["record"]["censored"] and not summaries["historical_E25_censored_acceptance"]["passed"]
for directory in directories.values():
	inputs = directory / ("inputs-after.json" if (directory / "inputs-after.json").exists() else "inputs-before.json")
	assert all(sha(Path(name)) == digest for name, digest in json.loads(inputs.read_text()).items())
parent = prepared / "parent"
candidate = prepared / "candidate"
assert sha(parent / "source.sha256") == "930997b62db85814718d0d5ac151f392bfae243a447d5d0d5f2adbde66cba5ae"
assert sha(candidate / "source.sha256") == "c20635425ff3d587fe939f62970d3d6370941013fcb2e85ee0da5e10c4a08907"
rows = [row.split("  ", 1) for row in (parent / "source.sha256").read_text().splitlines()]
assert len(rows) == 120
changed = sorted(name for digest, name in rows if sha(candidate / name) != digest)
assert changed == ["src/eval.c", "src/eval.h", "src/eval_internal.h", "src/eval_io.c"]
epoch.mkdir(exist_ok=False)
private = {}
for name, directory in directories.items():
	manifest = directory / "verification.sha256"
	assert not manifest.exists()
	files = sorted(path for path in directory.rglob("*") if path.is_file())
	manifest.write_text("".join(sha(path) + "  " + str(path.relative_to(directory)) + "\n" for path in files))
	private[name] = {"path": str(directory), "files": len(files), "manifest_sha256": sha(manifest)}
	copy(manifest, name + "-verification.sha256")
	for filename in ["summary.json", "inputs-before.json", "inputs-after.json"]:
		if (directory / filename).is_file(): copy(directory / filename, name + "-" + filename)
for name in changed:
	copy(candidate / name, "canonical/" + Path(name).name)
for variant in ["parent", "candidate"]:
	copy(prepared / variant / "source.sha256", variant + "-source.sha256")
copy(prepared / "accepted94-frame-order.patch", "accepted94-frame-order.patch")
copy(prepared / "source-preparation.json", "source-preparation.json")
for name in ["prepare_accepted_frame_port.py", "pin_accepted_frame_port.py", "accepted_verify.mk",
	"qualify_accepted_frame_focused.py", "qualify_accepted_frame_cuts.py", "qualify_accepted_frame_consumers.py",
	"qualify_accepted_frame_completed.py", "qualify_accepted_frame_partitions.py", "inspect_accepted_frame_layout.py",
	"inspect_accepted_frame_layout_retry.py", "readback_transport_bounds_test.c", "freeze_accepted_frame_epoch.py"]:
	copy(owner / name, name)
copy(owner / "accepted-frame-analysis.md", "analysis.md")
handoff = {"lane": "performance", "issue": "#56 / PR #58", "epoch": epoch.name,
	"status": "Focused source/correctness REVIEW; raw parent/paired byte failures explicitly unwaived, broader acceptance and actual net cost pending",
	"branch": "parallel/performance-20261003", "task_parent": "e71528fa286e263c568d4f78f3f1281f90b5e86d",
	"chosen_commit_message": "prototype: recycle stateless frames on accepted baseline",
	"accepted_baseline_revision": "94a20003b976475b1ed38025e61acb6cdbdab92c",
	"parent_source120": sha(parent / "source.sha256"), "candidate_source120": sha(candidate / "source.sha256"),
	"runtime_files": changed, "all116_other_runtime_files_and_Makefiles_exact": True,
	"every_added_removed_line_exact_E25_delta_accepted_reify_preserved": True,
	"patch_sha256": sha(epoch / "accepted94-frame-order.patch"), "private_evidence": private,
	"focused88_failed": 0, "consumer80_failed": 0, "raw_semantic_fuel_SAN2296_failed": 0,
	"raw_paired_images": 164, "raw_paired_byte_failures_unwaived": 20,
	"raw_parent_inert_byte_failures_unwaived": 36, "raw_candidate_inert_byte_failures": 0,
	"completed_profile230_unexpected_failures": 0, "public_profile12_pairs_96_inert_96_resume_checks_exact": True,
	"strict_recipes_failed": 2, "strict3_each_unwaived": True, "all52_images_full40_row_TSV_exact_current_pair": True,
	"six_completed_fuels": summaries["completed"]["six_fresh_accepted_completed_fuels"],
	"historical_E25_LocalSorted761848_retained_current956507_not_a_test_adapter": True,
	"layout_metadata_predicate_failures_original_acceptance_timeout_and_all_other_original_failures_retained": True,
	"all_children_stopped": True, "broader_current_acceptance_and_downstream_C_separate": True,
	"actual_RSS_time_cost_grant_runtime_READY_accepted_promotion_Goal_completion": False,
	"next": "New matched36 current accepted baseline versus full candidate, including ordering overhead; Merge exclusive grant required."}
(epoch / "handoff.json").write_text(json.dumps(handoff, indent=2) + "\n")
files = sorted(path for path in epoch.rglob("*") if path.is_file())
manifest = epoch / "manifest.sha256"
manifest.write_text("".join(sha(path) + "  " + str(path.relative_to(epoch)) + "\n" for path in files))
print(json.dumps({"manifest": str(manifest), "manifest_sha256": sha(manifest),
	"files_including_manifest": len(files) + 1, "private_evidence": private}), flush=True)
