import argparse
import datetime
import hashlib
import json
import os
import pathlib
import runpy
import shlex
import subprocess
import sys


def digest(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
	parser = argparse.ArgumentParser(description="Focused serial fixed-Acc command/product qualification.")
	for name in ("driver", "fault_driver", "image", "pointer", "core_cases", "output"):
		parser.add_argument(name, type=pathlib.Path)
	args = parser.parse_args()
	here = pathlib.Path(__file__).resolve().parent
	out = args.output.absolute()
	out.mkdir()
	rows = []
	inputs = [args.driver, args.fault_driver, args.image, args.pointer, args.core_cases,
		here / "compose.py", here / "check.py", here / "main.c", here / "fault.c", here / "build.mk",
		here.parent / "acc_products/client.c", here.parent / "acc_products/resource_client.c",
		here.parent / "acc_frame/map_client.c"]
	before = {str(p.resolve()): digest(p) for p in inputs}
	cc = os.environ.get("CC", "cc")
	flags = shlex.split(os.environ.get("CLIENT_CFLAGS", "-O2"))
	compile_prefix = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", *flags]
	compose = [sys.executable, str(here / "compose.py"), str(args.image.resolve())]
	common = ["--driver", str(args.driver.resolve()), "--cc", cc,
		*["--cflag=" + flag for flag in flags]]

	def run(label, expected, argv, extra_env=None):
		environment = dict(os.environ)
		if extra_env:
			environment.update(extra_env)
		start = datetime.datetime.now(datetime.timezone.utc).isoformat()
		with (out / (label + ".out")).open("wb") as stdout, (out / (label + ".err")).open("wb") as stderr:
			result = subprocess.run([str(x) for x in argv], stdout=stdout, stderr=stderr, env=environment)
		row = {"label": label, "expected": expected, "exit": result.returncode,
			"argv": [str(x) for x in argv], "start": start,
			"finish": datetime.datetime.now(datetime.timezone.utc).isoformat(),
			"environment": extra_env or {},
			"stdout_sha256": digest(out / (label + ".out")), "stderr_sha256": digest(out / (label + ".err"))}
		rows.append(row)
		(out / "commands.json").write_text(json.dumps(rows, indent=2) + "\n")
		assert result.returncode == expected, row

	def failed_product(label, expected, options=(), extra_env=None):
		destination = out / label
		run(label, expected, [*compose, str(destination), *common, *options], extra_env)
		assert not os.path.lexists(destination)
		assert not list(out.glob(".acc-command-*"))

	try:
		run("source_reference", 0, [args.pointer, "--load", "--steps", "5000000", "--run", "reference", args.image])
		# This is an adaptation of retained Core-generated cases, not a new source oracle.
		core = args.core_cases.read_text().replace('"mockup.h"', '"component.h"').replace("qs_mockup_sort", "gs_sort")
		(out / "core_cases.c").write_text(core)
		for product in ("source", "object", "archive"):
			directory = out / product
			run(product + "_command", 0, [*compose, str(directory), *common, "--product", product])
			operand = directory / {"source": "component.c", "object": "component.o", "archive": "library.a"}[product]
			for label, source, linker in (
				("client", here.parent / "acc_products/client.c", []),
				("resource", here.parent / "acc_products/resource_client.c", ["-Wl,--wrap=malloc"]),
				("Core", out / "core_cases.c", [])):
				binary = directory / label
				run(product + "_" + label + "_build", 0, [*compile_prefix, "-I" + str(directory), source, operand, *linker, "-o", binary])
				run(product + "_" + label, 0, [binary])
			assert (out / (product + "_client.out")).read_bytes() == (out / "source_reference.out").read_bytes()
			for name in ("component.c", "component.h", "provenance.json"):
				assert (directory / name).read_bytes() == (here.parent / "acc_frame/example" / name).read_bytes()
			receipt = json.loads((directory / "product.json").read_text())
			assert receipt["artifact_sha256"] == digest(args.image)
			assert receipt["driver_sha256"] == digest(args.driver)
			assert len(receipt["generated_inputs"]) == 8 and all(c["exit"] == 0 for c in receipt["commands"])
			assert all(digest(directory / name) == value for name, value in receipt["outputs"].items())
		directory = out / "source"
		run("map_build", 0, [*compile_prefix, "-I" + str(directory), here.parent / "acc_frame/map_client.c", "-o", out / "map_client"])
		run("map_client", 0, [out / "map_client"])
		run("public_symbols", 0, ["nm", "-g", "--defined-only", out / "object/component.o"])
		symbols = (out / "public_symbols.out").read_text().splitlines()
		assert len(symbols) == 1 and symbols[0].split()[-2:] == ["T", "gs_sort"]
		run("duplicate_symbol", 1, [*compile_prefix, "-I" + str(directory), here.parent / "acc_products/client.c",
			out / "object/component.o", out / "object/component.o", "-o", out / "duplicate"])
		run("successor_alias", 0, [*compose, str(out / "alias"), *common, "--successor", "exact_successor"])
		for name in ("component.c", "component.h", "provenance.json"):
			assert (out / "alias" / name).read_bytes() == (directory / name).read_bytes()
		failed_product("changed_successor", 4, ["--successor", "changed_successor"])
		failed_product("missing_successor", 4, ["--successor", "nonexistent_successor"])
		failed_product("no_fuel", 3, ["--steps", "0"])
		failed_product("unsupported_product", 4, ["--product", "native"])
		failed_product("missing_compiler", 2, ["--product", "object", "--cc", str(out / "missing-cc")])
		failed_product("compiler_refusal", 1, ["--product", "object", "--cflag=-facc-command-invalid-option"])
		failed_product("missing_archiver", 2, ["--product", "archive", "--ar", str(out / "missing-ar")])
		prior = {str(p.relative_to(directory)): digest(p) for p in directory.iterdir() if p.is_file()}
		run("prior_product", 2, [*compose, str(directory), *common])
		assert prior == {str(p.relative_to(directory)): digest(p) for p in directory.iterdir() if p.is_file()}
		(out / "empty-prior").mkdir()
		run("prior_empty", 2, [*compose, str(out / "empty-prior"), *common])
		assert not list((out / "empty-prior").iterdir())
		(out / "dangling-prior").symlink_to("never-created")
		run("prior_symlink", 2, [*compose, str(out / "dangling-prior"), *common])
		assert (out / "dangling-prior").is_symlink()
		for label, image in (("missing_image", out / "missing.a"), ("malformed_image", out / "malformed.a")):
			if label == "malformed_image":
				image.write_bytes(b"not an A Program artifact\n")
			run(label, 2, [sys.executable, str(here / "compose.py"), str(image), str(out / label), *common])
			assert not os.path.lexists(out / label)
		for name in ("open", "write", "close", "temporary"):
			failed_product("io_" + name, 2, ["--driver", str(args.fault_driver.resolve())], {"ACC_COMMAND_IO_FAULT": name})
		# A late pre-existing body must survive, while this invocation's earlier bodies disappear.
		stage = out / "prior-bodies"
		stage.mkdir()
		(stage / "clause.inc").write_bytes(b"prior clause\n")
		(stage / "unrelated").write_bytes(b"prior sibling\n")
		run("prior_body_cleanup", 2, [args.driver, args.image, stage, "succ_access", "5000000"])
		assert {p.name: p.read_bytes() for p in stage.iterdir()} == {"clause.inc": b"prior clause\n", "unrelated": b"prior sibling\n"}
		# Exercise the final no-replace operation after an output appeared during staging.
		staged = out / "publication-stage"
		staged.mkdir()
		(staged / "component.c").write_bytes(b"new code\n")
		publish = runpy.run_path(str(here / "compose.py"))["publish_directory"]
		try:
			publish(staged, out / "empty-prior")
		except FileExistsError:
			pass
		else:
			raise AssertionError("late output was overwritten")
		assert (staged / "component.c").read_bytes() == b"new code\n"
		assert not list((out / "empty-prior").iterdir())
		assert not list(out.glob(".acc-command-*"))
		assert before == {str(p.resolve()): digest(p) for p in inputs}
		(out / "verification.json").write_text(json.dumps({"commands": len(rows), "all_expected": True,
			"input_hashes": before, "flags": flags, "late_no_replace": True, "input_bytes_unchanged": True,
			"scope": "fixed actual Acc saved-image command, finite focused controls; no general native/adoption/cost"}, indent=2) + "\n")
		return 0
	finally:
		(out / "input-hashes.json").write_text(json.dumps(before, indent=2) + "\n")


if __name__ == "__main__":
	sys.exit(main())
