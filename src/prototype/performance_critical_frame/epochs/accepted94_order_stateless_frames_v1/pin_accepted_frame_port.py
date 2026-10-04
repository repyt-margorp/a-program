"""Pin only the reviewed four-file delta on committed accepted inputs."""
import difflib
import hashlib
import json
from pathlib import Path


owner = Path(__file__).resolve().parent
output = owner / "private/accepted94_order_stateless_frames_v1_20261004"
parent = output / "parent"
candidate = output / "candidate"
old = owner / "epochs/current_e25_order_stateless_frames_v1"


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def vectors(patch):
	result = {}
	name = None
	for line in patch.splitlines():
		if line.startswith("+++ b/"):
			name = line[6:]
			result[name] = {"+": [], "-": []}
		elif name and line[:1] in ["+", "-"] and not line.startswith(("+++", "---")):
			result[name][line[0]].append(line[1:])
	return result


pins = json.loads((output / "inputs-before.json").read_text())
assert all(sha(Path(name)) == digest for name, digest in pins.items())
rows = [row.split("  ", 1) for row in (parent / "source.sha256").read_text().splitlines()]
assert len(rows) == 120
changed = sorted(name for digest, name in rows if sha(candidate / name) != digest)
assert changed == ["src/eval.c", "src/eval.h", "src/eval_internal.h", "src/eval_io.c"]
patch = "".join("".join(difflib.unified_diff((parent / name).read_text().splitlines(keepends=True),
	(candidate / name).read_text().splitlines(keepends=True), fromfile="a/" + name, tofile="b/" + name))
	for name in changed)
assert vectors(patch) == vectors((old / "combined-current-E25.patch").read_text())
assert "reuse_closed" not in (candidate / "src/eval.c").read_text()
assert "if (!closure.environment) entry->result = closure.term;" in (candidate / "src/eval.c").read_text()
for name in ["Makefile", "src/Makefile"]:
	assert (parent / name).read_bytes() == (candidate / name).read_bytes()
for name in changed:
	for line in vectors(patch)[name]["+"]:
		assert line.isascii() and not line.startswith(" "), (name, line)
	pins[str(candidate / name)] = sha(candidate / name)
(output / "accepted94-frame-order.patch").write_text(patch)
(candidate / "source.sha256").write_text("".join(sha(candidate / name) + "  " + name + "\n" for _, name in rows))
pins[str(Path(__file__))] = sha(Path(__file__))
pins[str(output / "accepted94-frame-order.patch")] = sha(output / "accepted94-frame-order.patch")
pins[str(candidate / "source.sha256")] = sha(candidate / "source.sha256")
for _, name in rows:
	pins[str(candidate / name)] = sha(candidate / name)
report = {"accepted_revision": "94a20003b976475b1ed38025e61acb6cdbdab92c",
	"parent_source120": sha(parent / "source.sha256"), "candidate_source120": sha(candidate / "source.sha256"),
	"changed_runtime_files": changed, "all116_other_runtime_files_and_two_Makefiles_exact": True,
	"every_added_removed_line_per_file_exact_frozen_E25_delta": True,
	"accepted_reify_contract_no_old_reuse_closed_preserved": True,
	"tabs_and_English_ASCII_source_check": True, "manual_edit_method": "apply_patch",
	"patch_sha256": sha(output / "accepted94-frame-order.patch"),
	"branch": "parallel/performance-20261003", "task_parent": "e71528fa286e263c568d4f78f3f1281f90b5e86d",
	"chosen_future_commit_message": "prototype: recycle stateless frames on accepted baseline",
	"original_failures_retained_no_waivers": True,
	"compact_materialized_consumer_fuel_SAN_qualification_and_actual_peak_RSS_time_pending": True,
	"runtime_READY_publication_cost_grant_collector_promotion_Goal_completion": False}
(output / "inputs-after.json").write_text(json.dumps(pins, indent=2) + "\n")
(output / "source-preparation.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report), flush=True)
