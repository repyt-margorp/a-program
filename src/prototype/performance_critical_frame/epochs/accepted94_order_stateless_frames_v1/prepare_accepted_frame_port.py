"""Copy only committed accepted inputs for the dominant four-file port."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


owner = Path(__file__).resolve().parent
worker = owner.parents[2]
root = Path("/home/repyt/workspace/a-program-workers/merge-storage-20261003/accepted-job-promotion")
manifest = root / "src/prototype/solver_inputs/accepted_promotion/20261004/runtime.sha256"
revision = "94a20003b976475b1ed38025e61acb6cdbdab92c"
output = owner / "private/accepted94_order_stateless_frames_v1_20261004"
route = owner.parent / "coordination/inbox/performance-accepted-Job-current-port-review-20261004.json"
old = owner / "epochs/current_e25_order_stateless_frames_v1"
pins = {}
origins = {}


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
	path.write_text(json.dumps(value, indent=2) + "\n")


assert sha(manifest) == "930997b62db85814718d0d5ac151f392bfae243a447d5d0d5f2adbde66cba5ae"
rows = [row.split("  ", 1) for row in manifest.read_text().splitlines()]
assert len(rows) == 120
names = [name for _, name in rows] + ["Makefile", "src/Makefile"]
expected_blobs = subprocess.check_output(["git", "rev-parse", *[revision + ":" + name for name in names]],
	cwd=worker, text=True).splitlines()
actual_blobs = subprocess.check_output(["git", "hash-object", *[str(root / name) for name in names]],
	cwd=worker, text=True).splitlines()
assert actual_blobs == expected_blobs and len(actual_blobs) == 122
output.mkdir(exist_ok=False)
for variant in ["parent", "candidate"]:
	for name in names:
		path = root / name
		destination = output / variant / name
		destination.parent.mkdir(parents=True, exist_ok=True)
		shutil.copyfile(path, destination)
		assert sha(path) == sha(destination)
		pins[str(path)] = sha(path)
		origins[str(destination)] = {"path": str(path), "git_blob": actual_blobs[names.index(name)],
			"revision": revision, "sha256": sha(path)}
		if variant == "parent":
			pins[str(destination)] = sha(destination)
	shutil.copyfile(manifest, output / variant / "source.sha256")
	assert sha(output / variant / "source.sha256") == sha(manifest)
	if variant == "parent":
		pins[str(output / variant / "source.sha256")] = sha(manifest)
for digest, name in rows:
	assert sha(root / name) == digest
for path in [Path(__file__), manifest, route, old / "manifest.sha256", old / "combined-current-E25.patch",
	old / "frame-layer.patch", old / "order-layer.patch"]:
	pins[str(path)] = sha(path)
write_json(output / "input-origins.json", origins)
write_json(output / "inputs-before.json", pins)
write_json(output / "preparation.json", {"revision": revision, "parent_source120": sha(manifest),
	"parent": str(output / "parent"), "candidate": str(output / "candidate"),
	"all122_input_blobs_exact_committed_accepted_revision": True, "source_runtime_records": 120,
	"planned_runtime_files": ["src/eval.c", "src/eval.h", "src/eval_internal.h", "src/eval_io.c"],
	"old_E25_frozen_epoch_sha256": sha(old / "manifest.sha256"),
	"old_E25_cost_config_deferred_ungranted_immutable": True,
	"preserve_accepted_reify_contract_without_old_unaccepted_reuse_closed_or_Surface": True,
	"profile": "Accepted compact/recomputable --save and --save-inputs; explicit --save-materialized; decoder default1M unchanged",
	"runtime_edits_qualification_actual_RSS_time_cost_grant_promotion_Goal_complete": False})
print(json.dumps({"prepared": str(output), "runtime_records": 120, "parent_source120": sha(manifest),
	"all122_blobs_committed_exact": True, "accepted_sources_read_only": True}), flush=True)
