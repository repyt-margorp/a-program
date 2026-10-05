import argparse
import json
import os
import pathlib
import runpy
import subprocess
import sys
import tempfile


here = pathlib.Path(__file__).resolve().parent
command = runpy.run_path(str(here.parent / "acc_create_command/compose.py"))
CommandFailure = command["CommandFailure"]
digest = command["digest"]
run = command["run"]


def compose(args):
	script = args.script.absolute()
	output = args.output.absolute()
	parser = args.plan_driver.absolute()
	driver = args.driver.absolute()
	script_before = digest(script)
	parser_before = digest(parser)
	driver_before = digest(driver)
	plan_argv = [str(parser), str(script)]
	parsed = subprocess.run(plan_argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	if parsed.stderr:
		sys.stderr.buffer.write(parsed.stderr)
	if parsed.returncode:
		raise CommandFailure(parsed.returncode if parsed.returncode in (2, 4) else 2)
	plan = json.loads(parsed.stdout)
	image = pathlib.Path(plan["artifact"]).absolute()
	image_before = digest(image)
	if os.path.lexists(output):
		raise FileExistsError("output already exists: " + str(output))
	commands = [{"argv": plan_argv, "exit": parsed.returncode}]
	package = runpy.run_path(str(here / "pack.py"))["package"]
	with tempfile.TemporaryDirectory(prefix=".acc-link-", dir=output.parent) as temporary:
		stage = pathlib.Path(temporary)
		bodies = stage / "bodies"
		bodies.mkdir()
		run([str(driver), str(image), str(bodies), plan["successor"], plan["source"], str(args.steps)], commands)
		product = stage / "product"
		package(bodies, bodies / "transport.inc", product)
		if plan["product"] != "source":
			run([args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", *args.cflag,
				"-c", str(product / "component.c"), "-o", str(product / "component.o")], commands)
		if plan["product"] == "archive":
			run([args.ar, "rcs", str(product / "library.a"), str(product / "component.o")], commands)
		# Pin the declarative plan as well as the saved image. Publication must not
		# describe a different script/image/helper than this invocation consumed.
		if script_before != digest(script) or image_before != digest(image) or \
			parser_before != digest(parser) or driver_before != digest(driver):
			raise OSError("input changed during Acc candidate composition")
		receipt = {"scope": "fixed actual Acc closed-source comparator candidate; not generalized native lowering",
			"plan": plan, "link_script": str(script), "link_script_sha256": script_before,
			"artifact_sha256": image_before, "plan_driver_sha256": parser_before,
			"driver_sha256": driver_before, "steps_per_selector": args.steps,
			"admission": "ordinary-solve; fixed source roles; no trust-image mode",
			"commands": commands,
			"generated_inputs": {p.name: digest(p) for p in sorted(bodies.iterdir())},
			"outputs": {p.name: digest(p) for p in sorted(product.iterdir())},
			"limits": ["manual target storage/roles/closures/array adapter; full Scope/source equivalence unproved",
				"source-selected Acc/down/indices/partition/captures and LT prior/result creation; immediate prior only",
				"borrowed whole immutable graph/Nat32/depth256/node65536/nonoverlap/transactional output",
				"ordinary native indexed/callable Acc remains unsupported; no producer/schema/checker/erasure extension"]}
		(product / "product.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
		command["publish_directory"](product, output)


def main():
	parser = argparse.ArgumentParser(description="LinkerScript products for the labeled actual Acc C candidate.")
	parser.add_argument("script", type=pathlib.Path)
	parser.add_argument("output", type=pathlib.Path)
	parser.add_argument("--plan-driver", type=pathlib.Path, required=True)
	parser.add_argument("--driver", type=pathlib.Path, required=True)
	parser.add_argument("--steps", type=int, default=5000000)
	parser.add_argument("--cc", default="cc")
	parser.add_argument("--ar", default="ar")
	parser.add_argument("--cflag", action="append", default=[])
	args = parser.parse_args()
	if args.steps < 0 or args.steps > (1 << 64) - 1:
		parser.error("steps must be an unsigned 64-bit integer")
	try:
		compose(args)
	except CommandFailure as error:
		return error.status
	except ValueError as error:
		print(str(error), file=sys.stderr)
		return 4
	except OSError as error:
		print(str(error), file=sys.stderr)
		return 2
	return 0


if __name__ == "__main__":
	sys.exit(main())
