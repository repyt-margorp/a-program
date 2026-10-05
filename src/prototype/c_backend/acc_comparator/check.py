import argparse
import datetime
import hashlib
import json
import os
import pathlib
import shlex
import subprocess
import sys


def digest(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
	parser = argparse.ArgumentParser(description="Serial actual-Acc comparator capture qualification.")
	for name in ("plan_driver", "backend", "driver", "fault_driver", "image", "pointer", "core_cases", "output"):
		parser.add_argument(name, type=pathlib.Path)
	args = parser.parse_args()
	here = pathlib.Path(__file__).resolve().parent
	out = args.output.absolute()
	out.mkdir()
	scripts = out / "scripts with spaces"
	scripts.mkdir()
	image = scripts / "actual image.a"
	image.write_bytes(args.image.read_bytes())
	inputs = [args.plan_driver, args.backend, args.driver, args.fault_driver, args.image,
		args.pointer, args.core_cases, here / "plan.c", here / "compose.py", here / "actual.aplink", here / "runtime.c", here / "module.h", here / "client.c", here / "resource_client.c", here / "pack.py", here.parent / "acc_measure/emit.c", here.parent / "acc_measure/emit.h",
		here / "check.py", here / "build.mk", here.parent / "link/plan.c", here.parent / "link/plan.h",
		here.parent / "link/driver.c", here.parent / "main.c", here.parent / "acc_create_command/compose.py",
		here.parent / "acc_create_command/recipe_pack.py", here.parent / "acc_products/client.c",
		here.parent / "acc_products/resource_client.c", here.parent / "acc_create/map_client.c"]
	before = {str(p.resolve()): digest(p) for p in inputs}
	rows = []
	cc = os.environ.get("CC", "cc")
	flags = shlex.split(os.environ.get("CLIENT_CFLAGS", "-O2"))
	compile_prefix = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", *flags]
	common = ["--plan-driver", str(args.plan_driver.resolve()), "--driver", str(args.driver.resolve()),
		"--cc", cc, *["--cflag=" + flag for flag in flags]]
	compose = [sys.executable, str(here / "compose.py")]
	base = (here / "actual.aplink").read_text().replace("artifact actual.a", 'artifact "actual image.a"')

	def script(label, text):
		path = scripts / (label + ".aplink")
		path.write_text(text)
		return path

	def run(label, expected, argv, extra_env=None):
		env = dict(os.environ)
		if extra_env:
			env.update(extra_env)
		start = datetime.datetime.now(datetime.timezone.utc).isoformat()
		with (out / (label + ".out")).open("wb") as stdout, (out / (label + ".err")).open("wb") as stderr:
			result = subprocess.run([str(x) for x in argv], stdout=stdout, stderr=stderr, env=env)
		rows.append({"label": label, "expected": expected, "exit": result.returncode,
			"argv": [str(x) for x in argv], "start": start,
			"finish": datetime.datetime.now(datetime.timezone.utc).isoformat(),
			"environment": extra_env or {},
			"stdout_sha256": digest(out / (label + ".out")), "stderr_sha256": digest(out / (label + ".err"))})
		(out / "commands.json").write_text(json.dumps(rows, indent=2) + "\n")
		assert result.returncode == expected, rows[-1]

	def failure(label, expected, text, options=(), extra_env=None):
		path = script(label, text)
		destination = out / label
		run(label, expected, [*compose, path, destination, *common, *options], extra_env)
		assert not os.path.lexists(destination)
		assert not list(out.glob(".acc-link-*"))

	try:
		run("source_reference", 0, [args.pointer, "--load", "--steps", "5000000", "--run", "reference", args.image])
		run("source_descending", 0, [args.pointer, "--load", "--steps", "5000000", "--run", "reference_descending", args.image])
		# Retained Core cases are an existing oracle input, not a fresh native oracle.
		core = args.core_cases.read_text().replace('"mockup.h"', '"component.h"').replace("qs_mockup_sort", "gs_sort")
		(out / "core_cases.c").write_text(core)
		for product in ("source", "object", "archive"):
			path = script(product, base.replace("product source", "product " + product))
			directory = out / product
			run(product + "_command", 0, [*compose, path, directory, *common])
			operand = directory / {"source": "component.c", "object": "component.o", "archive": "library.a"}[product]
			for label, source, linker in (
				("client", here.parent / "acc_products/client.c", []),
				("resource", here.parent / "acc_products/resource_client.c", ["-Wl,--wrap=malloc"]),
				("Core", out / "core_cases.c", [])):
				binary = directory / label
				run(product + "_" + label + "_build", 0, [*compile_prefix, "-I" + str(directory), source, operand, *linker, "-o", binary])
				run(product + "_" + label, 0, [binary])
			assert (out / (product + "_client.out")).read_bytes() == (out / "source_reference.out").read_bytes()
			binary = directory / "comparator_client"
			run(product + "_comparator_build", 0, [*compile_prefix, "-I" + str(directory), here / "client.c", operand, "-o", binary])
			for mode, reference in (("ascending", "source_reference"), ("descending", "source_descending")):
				run(product + "_comparator_" + mode, 0, [binary, mode])
				assert (out / (product + "_comparator_" + mode + ".out")).read_bytes() == (out / (reference + ".out")).read_bytes()
			binary = directory / "comparator_resource"
			run(product + "_comparator_resource_build", 0, [*compile_prefix, "-I" + str(directory), here / "resource_client.c", operand, "-Wl,--wrap=malloc", "-o", binary])
			run(product + "_comparator_resource", 0, [binary])
			for name in ("component.c", "component.h", "provenance.json"):
				assert (directory / name).read_bytes() == (here / "example" / name).read_bytes()
			receipt = json.loads((directory / "product.json").read_text())
			assert receipt["artifact_sha256"] == digest(image) and receipt["link_script_sha256"] == digest(path)
			assert receipt["plan_driver_sha256"] == digest(args.plan_driver) and receipt["driver_sha256"] == digest(args.driver)
			assert receipt["plan"]["product"] == product and receipt["plan"]["abi"] == "c_acc_comparator_candidate_v1"
			assert len(receipt["generated_inputs"]) == 8 and all(c["exit"] == 0 for c in receipt["commands"])
			assert all(digest(directory / name) == value for name, value in receipt["outputs"].items())
			direct = out / (product + "_plan.json")
			run(product + "_plan", 0, [args.plan_driver, path])
			direct.write_bytes((out / (product + "_plan.out")).read_bytes())
			assert json.loads(direct.read_bytes()) == receipt["plan"]
		directory = out / "source"
		run("map_build", 0, [*compile_prefix, "-I" + str(directory), here.parent / "acc_create/map_client.c", "-o", out / "map_client"])
		run("map_client", 0, [out / "map_client"])
		run("public_symbols", 0, ["nm", "-g", "--defined-only", out / "object/component.o"])
		assert [x.split()[-2:] for x in (out / "public_symbols.out").read_text().splitlines()] == [["T", "gs_sort"], ["T", "gs_sort_with"]]
		run("duplicate_symbol", 1, [*compile_prefix, "-I" + str(directory), here.parent / "acc_products/client.c",
			out / "object/component.o", out / "object/component.o", "-o", out / "duplicate"])
		alias = script("alias", base.replace("succ_access successor", "exact_successor successor"))
		run("alias", 0, [*compose, alias, out / "alias", *common])
		reordered = script("reordered", base.replace("export outer_parameter gs_sort_with\nexport succ_access successor", "export succ_access successor\nexport outer_parameter gs_sort_with"))
		run("reordered", 0, [*compose, reordered, out / "reordered", *common])
		for name in ("component.c", "component.h", "provenance.json"):
			assert (out / "alias" / name).read_bytes() == (directory / name).read_bytes()
			assert (out / "reordered" / name).read_bytes() == (directory / name).read_bytes()
		failure("changed_successor", 4, base.replace("succ_access successor", "changed_successor successor"))
		failure("missing_successor", 4, base.replace("succ_access successor", "absent_successor successor"))
		failure("no_fuel", 3, base, ["--steps", "0"])
		for label, expected, text in (
			("wrong_profile", 4, base.replace("c_acc_comparator_candidate_v1", "c_scalar_v1").replace("acc_comparator_candidate_v1", "scalar_direct_v1")),
			("wrong_abi", 2, base.replace("c_acc_comparator_candidate_v1", "c_native_v1")),
			("missing_fallback", 2, base.replace("fallback reject\n", "")),
			("wrong_target", 2, base.replace("host-c11", "other")),
			("shared", 4, base.replace("product source", "product shared")),
			("executable", 4, base.replace("product source", "product executable") + "entry gs_sort_with\n"),
			("wrong_outer", 4, base.replace("outer_parameter gs_sort_with", "wrong_parameter gs_sort_with")),
			("unary_parameter", 4, base.replace("outer_parameter gs_sort_with", "unary_parameter gs_sort_with")),
			("closed_parameter", 4, base.replace("outer_parameter gs_sort_with", "outer_fn gs_sort_with")),
			("wrong_symbol", 4, base.replace("outer_parameter gs_sort_with", "outer_parameter other")),
			("missing_role", 4, base.replace("export succ_access successor\n", "")),
			("extra_role", 4, base + "export nat_access extra\n"),
			("duplicate_role", 2, base + "export nat_access successor\n"),
			("native_script", 4, base.replace("product source", "product executable") + "entry gs_sort_with\nnative_script absent.ld\n"),
			("layout_directive", 2, base + "nat32 nat_type Nat\n"),
			("nul_script", 2, base + "\x00\n"),
			("bad_quote", 2, base.replace('"actual image.a"', '"actual image.a')),
			("malformed_image", 2, base.replace('"actual image.a"', "malformed.a")),
			("missing_image", 2, base.replace('"actual image.a"', "absent.a"))):
			if label == "malformed_image":
				(scripts / "malformed.a").write_bytes(b"not an image\n")
			failure(label, expected, text)
		failure("missing_compiler", 2, base.replace("product source", "product object"), ["--cc", str(out / "missing-cc")])
		failure("compiler_refusal", 1, base.replace("product source", "product object"), ["--cflag=-facc-link-invalid-option"])
		failure("missing_archiver", 2, base.replace("product source", "product archive"), ["--ar", str(out / "missing-ar")])
		for name in ("open", "write", "close", "temporary"):
			failure("io_" + name, 2, base, ["--driver", str(args.fault_driver.resolve())], {"ACC_COMMAND_IO_FAULT": name})
		run("missing_script", 2, [*compose, scripts / "absent.aplink", out / "missing-script", *common])
		assert not (out / "missing-script").exists()
		if pathlib.Path("/dev/full").exists():
			run("plan_output_io", 2, ["bash", "-c", '"$1" "$2" > /dev/full', "plan-output",
				args.plan_driver, scripts / "source.aplink"])
		# Explicit native-tool fixtures exercise real staging/publication races.
		# Their operands are argv values, never concatenated shell commands.
		late = out / "late-output"
		tool = out / "late-cc"
		tool.write_text("#!" + sys.executable + "\nimport os, pathlib, sys\n" +
			"pathlib.Path(" + repr(str(late)) + ").mkdir()\n" +
			"os.execvp(" + repr(cc) + ", [" + repr(cc) + ", *sys.argv[1:]])\n")
		tool.chmod(0o700)
		object_plan = script("late", base.replace("product source", "product object"))
		run("late_output", 2, [*compose, object_plan, late, *common, "--cc", tool])
		assert late.is_dir() and not list(late.iterdir())
		assert not list(out.glob(".acc-link-*"))
		changed_plan = script("changed_script", base.replace("product source", "product object"))
		tool = out / "change-script-cc"
		tool.write_text("#!" + sys.executable + "\nimport os, pathlib, sys\n" +
			"with pathlib.Path(" + repr(str(changed_plan)) + ").open('a') as file:\n\tfile.write('# changed during compile\\n')\n" +
			"os.execvp(" + repr(cc) + ", [" + repr(cc) + ", *sys.argv[1:]])\n")
		tool.chmod(0o700)
		run("changed_script", 2, [*compose, changed_plan, out / "changed-script", *common, "--cc", tool])
		assert not (out / "changed-script").exists()
		assert changed_plan.read_text().endswith("# changed during compile\n")
		assert not list(out.glob(".acc-link-*"))
		prior = {p.name: digest(p) for p in directory.iterdir() if p.is_file()}
		run("prior_product", 2, [*compose, scripts / "source.aplink", directory, *common])
		assert prior == {p.name: digest(p) for p in directory.iterdir() if p.is_file()}
		(out / "prior-empty").mkdir()
		run("prior_empty", 2, [*compose, scripts / "source.aplink", out / "prior-empty", *common])
		assert not list((out / "prior-empty").iterdir())
		(out / "prior-symlink").symlink_to("absent")
		run("prior_symlink", 2, [*compose, scripts / "source.aplink", out / "prior-symlink", *common])
		assert (out / "prior-symlink").is_symlink()
		# An otherwise valid candidate plan must not fall through the main backend,
		# even if its artifact is absent. Source admission is not bypassed to emit it.
		absent = script("backend_candidate", base.replace('"actual image.a"', "absent.a"))
		run("backend_candidate_refusal", 4, [args.backend, "--link", absent, out / "backend-candidate"])
		assert not (out / "backend-candidate").exists()
		native = script("backend_native", (here.parent / "acc_products/native.aplink").read_text().replace("artifact actual.a", 'artifact "actual image.a"'))
		run("backend_native_refusal", 4, [args.backend, "--link", native, out / "backend-native"])
		assert not (out / "backend-native").exists()
		assert before == {str(p.resolve()): digest(p) for p in inputs} and digest(image) == digest(args.image)
		assert not list(out.glob(".acc-link-*"))
		(out / "verification.json").write_text(json.dumps({"all_expected": True, "commands": len(rows),
			"input_hashes": before, "flags": flags, "input_bytes_unchanged": True,
			"candidate_parent_prefix_exact_C54": True, "actual_comparator_capture": True, "ordinary_native_refusal_retained": True,
			"scope": "fixed actual comparator capture candidate; no generalized native/full61/adoption/cost"}, indent=2) + "\n")
	except BaseException:
		(out / "failed.json").write_text(json.dumps({"commands": len(rows), "input_hashes": before}, indent=2) + "\n")
		raise


if __name__ == "__main__":
	main()
