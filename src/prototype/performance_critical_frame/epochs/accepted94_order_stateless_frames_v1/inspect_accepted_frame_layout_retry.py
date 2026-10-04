"""Compare existing actual accepted layouts without importing historical offsets."""
import hashlib
import json
from pathlib import Path


owner = Path(__file__).resolve().parent
original = owner / "private/accepted94_order_stateless_frames_v1_layouts_20261004"
output = owner / "private/accepted94_order_stateless_frames_v1_layouts_retry_20261004"


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


pins = json.loads((original / "inputs-before.json").read_text())
assert all(sha(Path(name)) == digest for name, digest in pins.items())
records = json.loads((original / "records.json").read_text())
assert len(records) == 4 and all(row["exit"] == 0 for row in records)
layouts = {variant: json.loads((original / (variant + "-layout.json")).read_text())
	for variant in ["parent", "candidate"]}
for variant in layouts:
	assert not layouts[variant]["target_started"] and not layouts[variant]["inferior_calls"]
	assert len(layouts[variant]["types"]) == 9
	log = (original / (variant + "-inspect-only.log")).read_text()
	assert "READBACK_ENTRY_LAYOUT size=" + str(88 if variant == "parent" else 96) + " alignment=8" in log
for name, value in layouts["parent"]["types"].items():
	other = layouts["candidate"]["types"][name]
	assert all(value[key] == other[key] for key in ["size", "alignment", "existing_16_byte_allocation_extent"])
	if name != "pg_eval":
		assert value == other
	else:
		before, after = value["fields"], other["fields"]
		assert after["head_ready"]["offset_bytes"] == before["status"]["offset_bytes"] + before["status"]["size"]
		assert after["free_frames"]["offset_bytes"] == before["head_ready"]["offset_bytes"]
		assert {k: v for k, v in before.items() if k != "head_ready"} == {
			k: v for k, v in after.items() if k not in ["head_ready", "free_frames"]}
for path in [Path(__file__), *sorted(original.glob("*"))]:
	if path.is_file(): pins[str(path)] = sha(path)
output.mkdir(exist_ok=False)
(output / "inputs-after.json").write_text(json.dumps(pins, indent=2) + "\n")
report = {"terminal": True, "reused_original_compile_inspect_records": records, "new_target_or_compiler_run": False,
	"failed": 0, "layouts": layouts, "all_nine_sizes_alignments_extents_and_other_field_maps_equal": True,
	"actual_accepted_pg_eval_size_extent": [136, 144], "actual_accepted_pg_whnf_size_extent": [336, 336],
	"head_ready_offsets_parent_candidate": [120, 84], "free_frames_offset": 120,
	"entry_size_parent_candidate": [88, 96], "single_16_aligned_extent": [96, 96],
	"loader_array_and_writer_scratch_extra_each": "8*n bytes before rounding; net cost must include both",
	"historical_absolute_offset_predicate_failure_retained": str(original / "initial-layout-predicate-failure.json"),
	"all_pins_exact_after": True, "binary_ABI_or_capacity_peak_RSS_time_claim": False}
(output / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({"terminal": True, "nine_extents_equal": True, "summary_sha256": sha(output / "summary.json")}), flush=True)
