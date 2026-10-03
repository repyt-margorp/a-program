#!/usr/bin/env python3
"""Verify every TotalResult fixture cut with separate writer and reader processes."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("binary", type=Path)
	parser.add_argument("output", type=Path)
	args = parser.parse_args()
	args.output.mkdir(parents=True, exist_ok=False)
	binary = args.binary.resolve()
	report = {"binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
		"test_source_sha256": hashlib.sha256(Path(__file__).with_name("total_result_test.c").read_bytes()).hexdigest(),
		"comparative_measurements": False, "checks": []}

	def run(command, label):
		result = subprocess.run(command, capture_output=True, text=True, timeout=30)
		(args.output / f"{label}.log").write_text(result.stdout + result.stderr)
		if result.returncode:
			report["failure"] = {"command": command, "exit": result.returncode}
			(args.output / "checks.json").write_text(json.dumps(report, indent=2) + "\n")
			raise RuntimeError(f"{label} failed with exit {result.returncode}")
		return result.stdout

	unit = run([str(binary)], "unit")
	cases = [(int(case), int(count)) for case, count in re.findall(
		r"TotalResult case=(\d+): all (\d+) raw reload cuts passed", unit)]
	if [case for case, count in cases] != list(range(5)) or any(count < 1 for case, count in cases):
		raise ValueError("unit output does not identify all five fixture cut ranges")
	for case, count in cases:
		for cut in range(count):
			label = f"case-{case}-cut-{cut}"
			machine = args.output / f"{label}.machine"
			writer = [str(binary), "write", str(machine), str(cut), str(case)]
			reader = [str(binary), "read", str(machine)]
			written = run(writer, label + "-write")
			read = run(reader, label + "-read")
			if written.strip() != f"TotalResult writer: cut={cut} total={count - 1}" or \
				read.strip() != f"TotalResult fresh reader: total={count - 1}":
				raise ValueError(f"{label}: writer/reader steps disagree with the independent unit run")
			report["checks"].append({"case": case, "cut": cut, "raw_total": count - 1,
				"writer": writer, "reader": reader, "writer_exit": 0, "reader_exit": 0,
				"machine_sha256": hashlib.sha256(machine.read_bytes()).hexdigest()})
		print(f"case {case}: {count} fresh-process splits passed", flush=True)
	report["passed"] = True
	(args.output / "checks.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
	main()
