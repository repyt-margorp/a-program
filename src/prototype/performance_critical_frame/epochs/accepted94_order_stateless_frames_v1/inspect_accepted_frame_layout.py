"""Read actual accepted-pair DWARF layouts without starting a target."""
import hashlib
import json
import os
from pathlib import Path
import subprocess


owner = Path(__file__).resolve().parent
prepared = owner / "private/accepted94_order_stateless_frames_v1_20261004"
output = owner / "private/accepted94_order_stateless_frames_v1_layouts_20261004"
probe = owner.parent / "performance_frame_current/header_layout_probe.c"
helper = owner.parent / "performance_mem10/layout.gdb.py"
compiler = Path("/usr/bin/x86_64-linux-gnu-gcc-14")
gdb = Path("/usr/bin/gdb")
pins = json.loads((prepared / "inputs-after.json").read_text())
records = []
layouts = {}


def sha(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
	path.write_text(json.dumps(value, indent=2) + "\n")


def run(label, argv, environment):
	argv = [str(value) for value in argv]
	log = output / (label + ".log")
	with log.open("wb") as stream:
		result = subprocess.run(argv, env=environment, stdout=stream, stderr=subprocess.STDOUT, timeout=60)
	records.append({"label": label, "argv": argv, "exit": result.returncode, "log_sha256": sha(log)})
	write_json(output / "records.json", records)
	assert result.returncode == 0, log


assert all(sha(Path(name)) == digest for name, digest in pins.items())
for path in [Path(__file__), probe, helper, compiler, gdb]:
	pins[str(path)] = sha(path)
output.mkdir(exist_ok=False)
(output / "temporary").mkdir()
write_json(output / "inputs-before.json", pins)
environment = dict(os.environ, TMPDIR=str(output / "temporary"))
for variant in ["parent", "candidate"]:
	root = prepared / variant
	object_file = output / (variant + ".o")
	run(variant + "-compile-only", [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O0", "-g",
		"-fno-eliminate-unused-debug-types", "-I" + str(root / "src"), "-c", probe, "-o", object_file], environment)
	pins[str(object_file)] = sha(object_file)
	data = output / (variant + "-layout.json")
	run(variant + "-inspect-only", [gdb, "--batch", "-q", "-nx", "-iex", "set auto-load off",
		"-ex", "set pagination off", "-ex", "set may-call-functions off", "-ex", "source " + str(helper),
		"-ex", "python print('READBACK_ENTRY_LAYOUT size=%d alignment=%d' % (gdb.lookup_type('struct readback_entry').sizeof, gdb.lookup_type('struct readback_entry').alignof))",
		object_file], dict(environment, AP_LAYOUT_OUTPUT=str(data)))
	layouts[variant] = json.loads(data.read_text())
	assert not layouts[variant]["target_started"] and not layouts[variant]["inferior_calls"]
	assert len(layouts[variant]["types"]) == 9
for name, value in layouts["parent"]["types"].items():
	next_value = layouts["candidate"]["types"][name]
	assert all(value[key] == next_value[key] for key in ["size", "alignment", "existing_16_byte_allocation_extent"])
	if name != "pg_eval":
		assert value == next_value
	else:
		assert value["fields"]["head_ready"] == {"offset_bytes": 144, "size": 4}
		assert next_value["fields"]["head_ready"] == {"offset_bytes": 108, "size": 4}
		assert next_value["fields"]["free_frames"] == {"offset_bytes": 144, "size": 8}
		assert {k: v for k, v in value["fields"].items() if k != "head_ready"} == {
			k: v for k, v in next_value["fields"].items() if k not in ["head_ready", "free_frames"]}
assert all(sha(Path(name)) == digest for name, digest in pins.items())
write_json(output / "inputs-after.json", pins)
report = {"terminal": True, "records": records, "failed": 0,
	"layouts": layouts, "all_nine_primary_sizes_alignments_extents_equal": True,
	"readback_entry_size_parent_candidate": [88, 96], "single_16_aligned_extents": [96, 96],
	"new_loader_array_and_writer_pointer_scratch_each": "8*n bytes before rounding; included in net cost candidate",
	"all_pins_exact_after": True, "target_started_inferior_calls_capacity_RSS_time_ABI_claim": False}
write_json(output / "summary.json", report)
print(json.dumps({"terminal": True, "records": 4, "nine_extents_equal": True,
	"summary_sha256": sha(output / "summary.json")}), flush=True)
