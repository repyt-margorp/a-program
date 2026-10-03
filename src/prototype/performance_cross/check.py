#!/usr/bin/env python3
"""Check prepared controls with explicit commands; collect no time/RSS metrics."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("controls", type=Path)
	parser.add_argument("commands", type=Path, help="JSON keyed by bend/lean/agda/v, with argv and env")
	parser.add_argument("output", type=Path)
	parser.add_argument("--timeout", type=int, default=30)
	args = parser.parse_args()
	args.output.mkdir(parents=True, exist_ok=False)
	manifest = json.loads((args.controls / "sources.json").read_text())
	commands = json.loads(args.commands.read_text())
	allowed_env = {"BEND_NO_TELEMETRY", "Agda_datadir", "OCAMLPATH", "OCAMLFIND_CONF",
		"COQLIB", "ROCQLIB", "BENDTT", "LEAN_STACK_SIZE_KB"}
	rejection = {"bend": "SOME PROOFS FAIL", "lean": "Not a definitional equality",
		"agda": "when checking that the expression refl has type", "v": "Unable to unify"}
	report = {"revision": manifest["revision"], "equalities": manifest["equalities"],
		"qualification": manifest.get("qualification", "prepared controls"),
		"commands_sha256": hashlib.sha256(args.commands.read_bytes()).hexdigest(),
		"comparative_measurements": False, "checks": []}
	failed = False
	for item in manifest["files"]:
		extension, case = item["extension"], item["case"]
		if extension not in commands:
			continue
		file = (args.controls / item["path"]).resolve()
		if hashlib.sha256(file.read_bytes()).hexdigest() != item["sha256"]:
			raise ValueError(f"control changed after preparation: {file}")
		config = commands[extension]
		if not set(config.get("env", {})) <= allowed_env:
			raise ValueError("unsupported environment override")
		environment = dict(os.environ)
		environment.update(config.get("env", {}))
		command = [str(file) if word == "{source}" else word for word in config["argv"]]
		try:
			result = subprocess.run(command, cwd=file.parent, env=environment,
				capture_output=True, text=True, timeout=args.timeout)
			exit_code = result.returncode
			log = result.stdout + result.stderr
		except subprocess.TimeoutExpired as error:
			exit_code = None
			log = "Control timed out; qualification failed.\n"
			for output in (error.stdout, error.stderr):
				log += output.decode(errors="replace") if isinstance(output, bytes) else (output or "")
		except OSError as error:
			exit_code = None
			log = f"Control could not run; qualification failed: {error}\n"
		(args.output / f"{extension}-{case}.log").write_text(log)
		passed = exit_code == 0 if item["expected_pass"] else (
			exit_code is not None and exit_code > 0 and rejection[extension] in log)
		report["checks"].append({"extension": extension, "case": case,
			"source_sha256": item["sha256"], "command": command,
			"environment_overrides": config.get("env", {}), "exit": exit_code,
			"expected_pass": item["expected_pass"], "qualified": passed})
		failed |= not passed
		print(f"{extension} {case}: exit {exit_code}, qualified={passed}")
	(args.output / "checks.json").write_text(json.dumps(report, indent=2) + "\n")
	if failed or not report["checks"]:
		raise SystemExit(1)


if __name__ == "__main__":
	main()
