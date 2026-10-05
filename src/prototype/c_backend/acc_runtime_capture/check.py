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
	parser = argparse.ArgumentParser(description="Serial actual Acc runtime source-parameter C qualification.")
	for name in ("driver", "fault_driver", "closed_driver", "backend", "image", "pointer", "core_cases", "output"):
		parser.add_argument(name, type=pathlib.Path)
	args = parser.parse_args()
	here = pathlib.Path(__file__).resolve().parent
	out = args.output.absolute()
	out.mkdir()
	inputs = [args.driver, args.fault_driver, args.closed_driver, args.backend, args.image,
		args.pointer, args.core_cases, *[here / name for name in
		("emit.c", "emit.h", "main.c", "build.mk", "pack.py", "compose.py", "runtime.c", "module.h",
		"client.c", "resource_client.c", "extra.p", "check.py")],
		here.parent / "acc_closed_capture/emit.c", here.parent / "acc_closed_capture/emit.h",
		here.parent / "acc_closed_capture/resource_client.c", here.parent / "acc_create_command/recipe_pack.py"]
	before = {str(p.resolve()): digest(p) for p in inputs}
	rows = []
	cc = os.environ.get("CC", "cc")
	flags = shlex.split(os.environ.get("CLIENT_CFLAGS", "-O2"))
	compile_prefix = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", *flags]
	compose = [sys.executable, str(here / "compose.py"), str(args.image.resolve())]
	options = ["--driver", str(args.driver.resolve()), "--cc", cc, *["--cflag=" + flag for flag in flags]]

	def run(label, expected, argv, extra_env=None):
		env = dict(os.environ)
		if extra_env:
			env.update(extra_env)
		start = datetime.datetime.now(datetime.timezone.utc).isoformat()
		with (out / (label + ".out")).open("wb") as stdout, (out / (label + ".err")).open("wb") as stderr:
			result = subprocess.run([str(x) for x in argv], stdout=stdout, stderr=stderr, env=env)
		rows.append({"label": label, "expected": expected, "exit": result.returncode,
			"argv": [str(x) for x in argv], "start": start,
			"finish": datetime.datetime.now(datetime.timezone.utc).isoformat(), "environment": extra_env or {},
			"stdout_sha256": digest(out / (label + ".out")), "stderr_sha256": digest(out / (label + ".err"))})
		(out / "commands.json").write_text(json.dumps(rows, indent=2) + "\n")
		assert result.returncode == expected, rows[-1]

	def failure(label, expected, extra=(), env=None):
		destination = out / label
		run(label, expected, [*compose, destination, *options, *extra], env)
		assert not os.path.lexists(destination) and not list(out.glob(".acc-command-*"))

	try:
		for selected, references, directions in (
			("runtime_sort", ("runtime_true", "runtime_false"), ("descending", "ascending")),
			("opposite_runtime_sort", ("opposite_runtime_true", "opposite_runtime_false"), ("ascending", "descending"))):
			for mode, reference in zip(("first", "second"), references):
				run(selected + "_" + mode + "_source", 0, [args.pointer, "--load", "--steps", "5000000", "--run", reference, args.image])
			for product in ("source", "object", "archive"):
				label = selected + "_" + product
				directory = out / label
				run(label + "_command", 0, [*compose, directory, *options, "--source", selected, "--product", product])
				operand = directory / {"source": "component.c", "object": "component.o", "archive": "library.a"}[product]
				for role, source, linker in (("client", here / "client.c", []), ("resource", here / "resource_client.c", ["-Wl,--wrap=malloc"])):
					binary = directory / role
					# Each binary accepts both data values; there is no recompilation
					# between modes, nor a caller-provided comparison interpretation.
					run(label + "_" + role + "_build", 0, [*compile_prefix, "-I" + str(directory), source, operand, *linker, "-o", binary])
					for mode, direction in zip(("first", "second"), directions):
						run(label + "_" + role + "_" + mode, 0, [binary, mode, direction])
				for mode, direction in zip(("first", "second"), directions):
					assert (out / (label + "_client_" + mode + ".out")).read_bytes() == (out / (selected + "_" + mode + "_source.out")).read_bytes()
					core = args.core_cases.read_text().replace('"mockup.h"', '"component.h"').replace("qs_mockup_sort", "core_mode")
					value = "GS_BOOL_FIRST" if mode == "first" else "GS_BOOL_SECOND"
					bridge = "static int core_mode(const uint32_t *input,size_t count,uint32_t *buffer,size_t capacity,size_t *written,struct qs_trace *trace)\n{\n\tint status=gs_sort_mode(" + value + ",input,count,buffer,capacity,written,trace);\n"
					if direction == "descending":
						bridge += "\tif (!status) for (size_t i=0;i<*written/2;++i) { uint32_t n=buffer[i];buffer[i]=buffer[*written-1-i];buffer[*written-1-i]=n; }\n"
					bridge += "\treturn status;\n}\n"
					core = core.replace("#include <assert.h>", "#include <assert.h>\n" + bridge)
					core_path = directory / ("Core_" + mode + ".c")
					core_path.write_text(core)
					binary = directory / ("Core_" + mode)
					run(label + "_Core_" + mode + "_build", 0, [*compile_prefix, "-I" + str(directory), core_path, operand, "-o", binary])
					run(label + "_Core_" + mode, 0, [binary])
				receipt = json.loads((directory / "product.json").read_text())
				assert receipt["abi"] == "c_acc_runtime_bool_candidate_v1" and receipt["source"] == selected
				assert receipt["artifact_sha256"] == digest(args.image) and receipt["driver_sha256"] == digest(args.driver)
				assert len(receipt["generated_inputs"]) == 9 and all(row["exit"] == 0 for row in receipt["commands"])
				assert all(digest(directory / name) == value for name, value in receipt["outputs"].items())
				assert (directory / "component.h").read_bytes() == (here / "module.h").read_bytes()
				if product != "source":
					for name in ("component.c", "component.h", "provenance.json"):
						assert (directory / name).read_bytes() == (out / (selected + "_source") / name).read_bytes()
				elif selected == "runtime_sort":
					for name in ("component.c", "component.h", "provenance.json"):
						assert (directory / name).read_bytes() == (here / "example" / name).read_bytes()
		for selected in ("unused_runtime_sort", "nat_runtime_sort", "bad_runtime_sort", "extra_runtime_sort",
			"wrong_runtime_result", "descending_sort", "outer_parameter", "absent_source"):
			failure("unsupported_" + selected, 4, ["--source", selected])
		failure("changed_successor", 4, ["--successor", "changed_successor"])
		failure("missing_successor", 4, ["--successor", "absent_successor"])
		failure("no_fuel", 3, ["--steps", "0"])
		failure("wrong_product", 4, ["--product", "shared"])
		failure("compiler_refusal", 1, ["--product", "object", "--cc", "/bin/false"])
		failure("compiler_missing", 2, ["--product", "object", "--cc", str(out / "absent-cc")])
		failure("archiver_refusal", 1, ["--product", "archive", "--ar", "/bin/false"])
		for mode in ("open", "write", "close", "temporary"):
			failure("io_" + mode, 2, ["--driver", args.fault_driver], {"ACC_COMMAND_IO_FAULT": mode})
		directory = out / "runtime_sort_source"
		prior = {p.name: digest(p) for p in directory.iterdir() if p.is_file()}
		run("prior_product", 2, [*compose, directory, *options])
		assert prior == {p.name: digest(p) for p in directory.iterdir() if p.is_file()}
		for label in ("prior_empty", "prior_symlink"):
			p = out / label
			if label == "prior_empty":
				p.mkdir()
			else:
				p.symlink_to("absent")
			run(label, 2, [*compose, p, *options])
		assert not list((out / "prior_empty").iterdir()) and (out / "prior_symlink").is_symlink()
		late = out / "late-output"
		tool = out / "late-cc"
		tool.write_text("#!" + sys.executable + "\nimport os,pathlib,sys\npathlib.Path(" + repr(str(late)) + ").mkdir()\nos.execvp(" + repr(cc) + ", [" + repr(cc) + ", *sys.argv[1:]])\n")
		tool.chmod(0o700)
		run("late_output", 2, [*compose, late, *options, "--product", "object", "--cc", tool])
		assert late.is_dir() and not list(late.iterdir())
		run("symbols", 0, ["nm", "-g", "--defined-only", out / "runtime_sort_object/component.o"])
		assert [x.split()[-2:] for x in (out / "symbols.out").read_text().splitlines()] == [["T", "gs_sort_mode"]]
		run("duplicate_symbol", 1, [*compile_prefix, "-I" + str(directory), here / "client.c",
			out / "runtime_sort_object/component.o", out / "runtime_sort_object/component.o", "-o", out / "duplicate"])
		stage = out / "closed-refusal"
		stage.mkdir()
		run("old_closed_refusal", 4, [args.closed_driver, args.image, stage, "succ_access", "runtime_sort", "5000000"])
		assert not list(stage.iterdir())
		script = out / "native.aplink"
		script.write_text((here.parent / "acc_products/native.aplink").read_text().replace("artifact actual.a", "artifact " + str(args.image.resolve())))
		run("ordinary_native_refusal", 4, [args.backend, "--link", script, out / "native"])
		assert not (out / "native").exists()
		assert before == {str(p.resolve()): digest(p) for p in inputs}
		assert not list(out.glob(".acc-command-*"))
		(out / "verification.json").write_text(json.dumps({"all_expected": True, "commands": len(rows),
			"input_hashes": before, "flags": flags, "input_bytes_unchanged": True,
			"actual_runtime_Bool_parameter_and_two_Acc_branches": True,
			"source_true_false_and_opposite_maps_match": True,
			"ordinary_native_and_closed_refusals_retained": True,
			"Core_scope": "retained finite ascending oracle; descending reversal composition, not fresh source oracle",
			"scope": "separate explicit target runtime candidate ABI; no producer/general native/full61/adoption/cost"}, indent=2) + "\n")
	except BaseException:
		(out / "failed.json").write_text(json.dumps({"commands": len(rows), "input_hashes": before}, indent=2) + "\n")
		raise


if __name__ == "__main__":
	main()
