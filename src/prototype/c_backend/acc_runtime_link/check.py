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
	parser = argparse.ArgumentParser(description="Serial actual Acc runtime source-parameter LinkerScript qualification.")
	for name in ("plan_driver", "publication_guard", "driver", "fault_driver", "closed_driver", "backend", "image", "pointer", "core_cases", "output"):
		parser.add_argument(name, type=pathlib.Path)
	args = parser.parse_args()
	here = pathlib.Path(__file__).resolve().parent
	out = args.output.absolute()
	out.mkdir()
	scripts = out / "scripts with spaces"
	scripts.mkdir()
	image = scripts / "actual image.a"
	image.write_bytes(args.image.read_bytes())
	parent = here.parent / "acc_runtime_capture"
	inputs = [args.plan_driver, args.publication_guard, args.driver, args.fault_driver, args.closed_driver, args.backend, args.image,
		args.pointer, args.core_cases, *[here / name for name in
		("plan.c", "publish_refusal.c", "build.mk", "compose.py", "actual.aplink", "check.py")],
		*[parent / name for name in ("emit.c", "emit.h", "main.c", "build.mk", "pack.py", "compose.py", "runtime.c", "module.h",
		"client.c", "resource_client.c", "extra.p", "check.py")],
		here.parent / "link/plan.c", here.parent / "link/plan.h", here.parent / "link/driver.c", here.parent / "main.c",
		here.parent / "acc_closed_capture/emit.c", here.parent / "acc_closed_capture/emit.h",
		here.parent / "acc_closed_capture/resource_client.c", here.parent / "acc_create_command/recipe_pack.py"]
	before = {str(p.resolve()): digest(p) for p in inputs}
	rows = []
	cc = os.environ.get("CC", "cc")
	flags = shlex.split(os.environ.get("CLIENT_CFLAGS", "-O2"))
	compile_prefix = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", *flags]
	compose = [sys.executable, str(here / "compose.py")]
	options = ["--plan-driver", str(args.plan_driver.resolve()), "--driver", str(args.driver.resolve()), "--cc", cc, *["--cflag=" + flag for flag in flags]]

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
			"finish": datetime.datetime.now(datetime.timezone.utc).isoformat(), "environment": extra_env or {},
			"stdout_sha256": digest(out / (label + ".out")), "stderr_sha256": digest(out / (label + ".err"))})
		(out / "commands.json").write_text(json.dumps(rows, indent=2) + "\n")
		assert result.returncode == expected, rows[-1]

	def failure(label, expected, text, extra=(), env=None):
		path = script(label, text)
		destination = out / label
		run(label, expected, [*compose, path, destination, *options, *extra], env)
		assert not os.path.lexists(destination) and not list(out.glob(".acc-link-*"))

	try:
		run("publication_guard", 0, [args.publication_guard])
		for selected, references, directions in (
			("runtime_sort", ("runtime_true", "runtime_false"), ("descending", "ascending")),
			("opposite_runtime_sort", ("opposite_runtime_true", "opposite_runtime_false"), ("ascending", "descending"))):
			for mode, reference in zip(("first", "second"), references):
				run(selected + "_" + mode + "_source", 0, [args.pointer, "--load", "--steps", "5000000", "--run", reference, args.image])
			for product in ("source", "object", "archive"):
				label = selected + "_" + product
				directory = out / label
				path = script(label, base.replace("runtime_sort gs_sort_mode", selected + " gs_sort_mode").replace("product source", "product " + product))
				run(label + "_command", 0, [*compose, path, directory, *options])
				operand = directory / {"source": "component.c", "object": "component.o", "archive": "library.a"}[product]
				for role, source, linker in (("client", parent / "client.c", []), ("resource", parent / "resource_client.c", ["-Wl,--wrap=malloc"])):
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
				assert receipt["plan"]["abi"] == "c_acc_runtime_bool_candidate_v1" and receipt["plan"]["source"] == selected
				assert receipt["artifact_sha256"] == digest(args.image) and receipt["driver_sha256"] == digest(args.driver)
				assert receipt["link_script_sha256"] == digest(path) and receipt["plan_driver_sha256"] == digest(args.plan_driver)
				run(label + "_plan", 0, [args.plan_driver, path])
				assert json.loads((out / (label + "_plan.out")).read_bytes()) == receipt["plan"]
				assert len(receipt["generated_inputs"]) == 9 and all(row["exit"] == 0 for row in receipt["commands"])
				assert all(digest(directory / name) == value for name, value in receipt["outputs"].items())
				assert (directory / "component.h").read_bytes() == (parent / "module.h").read_bytes()
				if product != "source":
					for name in ("component.c", "component.h", "provenance.json"):
						assert (directory / name).read_bytes() == (out / (selected + "_source") / name).read_bytes()
				elif selected == "runtime_sort":
					for name in ("component.c", "component.h", "provenance.json"):
						assert (directory / name).read_bytes() == (parent / "example" / name).read_bytes()
		for selected in ("unused_runtime_sort", "nat_runtime_sort", "bad_runtime_sort", "extra_runtime_sort",
			"wrong_runtime_result", "descending_sort", "outer_parameter", "absent_source"):
			failure("unsupported_" + selected, 4, base.replace("runtime_sort gs_sort_mode", selected + " gs_sort_mode"))
		failure("changed_successor", 4, base.replace("succ_access successor", "changed_successor successor"))
		failure("missing_successor", 4, base.replace("succ_access successor", "absent_successor successor"))
		for label, text in (
			("alias", base.replace("succ_access successor", "exact_successor successor")),
			("reordered", base.replace("export runtime_sort gs_sort_mode\nexport succ_access successor", "export succ_access successor\nexport runtime_sort gs_sort_mode"))):
			path = script(label, text)
			run(label, 0, [*compose, path, out / label, *options])
			for name in ("component.c", "component.h", "provenance.json"):
				assert (out / label / name).read_bytes() == (out / "runtime_sort_source" / name).read_bytes()
		failure("no_fuel", 3, base, ["--steps", "0"])
		failure("wrong_product", 4, base.replace("product source", "product shared"))
		failure("compiler_refusal", 1, base.replace("product source", "product object"), ["--cc", "/bin/false"])
		failure("compiler_missing", 2, base.replace("product source", "product object"), ["--cc", str(out / "absent-cc")])
		failure("archiver_refusal", 1, base.replace("product source", "product archive"), ["--ar", "/bin/false"])
		for mode in ("open", "write", "close", "temporary"):
			failure("io_" + mode, 2, base, ["--driver", args.fault_driver], {"ACC_COMMAND_IO_FAULT": mode})
		directory = out / "runtime_sort_source"
		prior = {p.name: digest(p) for p in directory.iterdir() if p.is_file()}
		run("prior_product", 2, [*compose, scripts / "runtime_sort_source.aplink", directory, *options])
		assert prior == {p.name: digest(p) for p in directory.iterdir() if p.is_file()}
		for label in ("prior_empty", "prior_symlink"):
			p = out / label
			if label == "prior_empty":
				p.mkdir()
			else:
				p.symlink_to("absent")
			run(label, 2, [*compose, scripts / "runtime_sort_source.aplink", p, *options])
		assert not list((out / "prior_empty").iterdir()) and (out / "prior_symlink").is_symlink()
		late = out / "late-output"
		tool = out / "late-cc"
		tool.write_text("#!" + sys.executable + "\nimport os,pathlib,sys\npathlib.Path(" + repr(str(late)) + ").mkdir()\nos.execvp(" + repr(cc) + ", [" + repr(cc) + ", *sys.argv[1:]])\n")
		tool.chmod(0o700)
		late_plan = script("late", base.replace("product source", "product object"))
		run("late_output", 2, [*compose, late_plan, late, *options, "--cc", tool])
		assert late.is_dir() and not list(late.iterdir())
		run("symbols", 0, ["nm", "-g", "--defined-only", out / "runtime_sort_object/component.o"])
		assert [x.split()[-2:] for x in (out / "symbols.out").read_text().splitlines()] == [["T", "gs_sort_mode"]]
		run("duplicate_symbol", 1, [*compile_prefix, "-I" + str(directory), parent / "client.c",
			out / "runtime_sort_object/component.o", out / "runtime_sort_object/component.o", "-o", out / "duplicate"])
		stage = out / "closed-refusal"
		stage.mkdir()
		run("old_closed_refusal", 4, [args.closed_driver, args.image, stage, "succ_access", "runtime_sort", "5000000"])
		assert not list(stage.iterdir())
		native_plan = out / "native.aplink"
		native_plan.write_text((here.parent / "acc_products/native.aplink").read_text().replace("artifact actual.a", "artifact " + str(args.image.resolve())))
		run("ordinary_native_refusal", 4, [args.backend, "--link", native_plan, out / "native"])
		assert not (out / "native").exists()
		for label, expected, text in (
			("wrong_profile", 4, base.replace("c_acc_runtime_bool_candidate_v1", "c_scalar_v1").replace("acc_runtime_bool_candidate_v1", "scalar_direct_v1")),
			("wrong_abi", 2, base.replace("c_acc_runtime_bool_candidate_v1", "c_native_v1")),
			("missing_fallback", 2, base.replace("fallback reject\n", "")),
			("wrong_target", 2, base.replace("host-c11", "other")),
			("shared", 4, base.replace("product source", "product shared")),
			("executable", 4, base.replace("product source", "product executable") + "entry gs_sort_mode\n"),
			("wrong_outer", 4, base.replace("runtime_sort gs_sort_mode", "unused_runtime_sort gs_sort_mode")),
			("wrong_symbol", 4, base.replace("runtime_sort gs_sort_mode", "runtime_sort other")),
			("missing_role", 4, base.replace("export succ_access successor\n", "")),
			("extra_role", 4, base + "export nat_access extra\n"),
			("duplicate_role", 2, base + "export nat_access successor\n"),
			("native_script", 4, base.replace("product source", "product executable") + "entry gs_sort_mode\nnative_script absent.ld\n"),
			("layout_directive", 2, base + "nat32 nat_type Nat\n"),
			("nul_script", 2, base + "\x00\n"),
			("bad_quote", 2, base.replace('"actual image.a"', '"actual image.a')),
			("malformed_image", 2, base.replace('"actual image.a"', "malformed.a")),
			("missing_image", 2, base.replace('"actual image.a"', "absent.a"))):
			if label == "malformed_image":
				(scripts / "malformed.a").write_bytes(b"not an image\n")
			failure(label, expected, text)
		run("missing_script", 2, [*compose, scripts / "absent.aplink", out / "missing-script", *options])
		assert not (out / "missing-script").exists()
		if pathlib.Path("/dev/full").exists():
			run("plan_output_io", 2, ["bash", "-c", '"$1" "$2" > /dev/full', "plan-output",
				args.plan_driver, scripts / "runtime_sort_source.aplink"])
		changed_plan = script("changed_script", base.replace("product source", "product object"))
		tool = out / "change-script-cc"
		tool.write_text("#!" + sys.executable + "\nimport os, pathlib, sys\n" +
			"with pathlib.Path(" + repr(str(changed_plan)) + ").open('a') as file:\n\tfile.write('# changed during compile\\n')\n" +
			"os.execvp(" + repr(cc) + ", [" + repr(cc) + ", *sys.argv[1:]])\n")
		tool.chmod(0o700)
		run("changed_script", 2, [*compose, changed_plan, out / "changed-script", *options, "--cc", tool])
		assert not (out / "changed-script").exists()
		assert changed_plan.read_text().endswith("# changed during compile\n")
		assert not list(out.glob(".acc-link-*"))
		# Mutate private copies only. Pinned qualified images/helpers stay exact.
		for name in ("image", "plan_driver", "driver"):
			original = {"image": args.image, "plan_driver": args.plan_driver, "driver": args.driver}[name]
			copy = scripts / ("mutable-" + name)
			copy.write_bytes(original.read_bytes())
			copy.chmod(0o700)
			text = base.replace("product source", "product object")
			extra = []
			if name == "image":
				text = text.replace('"actual image.a"', '"' + copy.name + '"')
			else:
				extra = ["--" + name.replace("_", "-"), copy]
			path = script("changed_" + name, text)
			tool = out / ("change-" + name + "-cc")
			tool.write_text("#!" + sys.executable + "\nimport os,pathlib,sys\n" +
				"with pathlib.Path(" + repr(str(copy)) + ").open('ab') as file:\n\tfile.write(b'changed-after-use')\n" +
				"os.execvp(" + repr(cc) + ", [" + repr(cc) + ", *sys.argv[1:]])\n")
			tool.chmod(0o700)
			run("changed_" + name, 2, [*compose, path, out / ("changed-" + name), *options, *extra, "--cc", tool])
			assert digest(copy) != digest(original) and not (out / ("changed-" + name)).exists()
			assert not list(out.glob(".acc-link-*"))
		absent = script("backend_candidate", base.replace('"actual image.a"', "absent.a"))
		run("backend_candidate_refusal", 4, [args.backend, "--link", absent, out / "backend-candidate"])
		assert not (out / "backend-candidate").exists()
		unreadable = scripts / "unreadable.a"
		unreadable.mkdir()
		for profile, abi in (("acc_runtime_bool_candidate_v1", "c_acc_runtime_bool_candidate_v1"),
			("acc_creation_candidate_v1", "c_acc_candidate_v1"),
			("acc_comparator_candidate_v1", "c_acc_comparator_candidate_v1")):
			text = base.replace("c_acc_runtime_bool_candidate_v1", abi).replace("lowering acc_runtime_bool_candidate_v1", "lowering " + profile)
			for artifact in ("absent.a", "unreadable.a"):
				path = script(profile + artifact, text.replace('"actual image.a"', artifact))
				prior = {p.name: digest(p) for p in directory.iterdir() if p.is_file()}
				run("early_" + profile + artifact, 4, [args.backend, "--link", path, directory])
				assert prior == {p.name: digest(p) for p in directory.iterdir() if p.is_file()}
		assert before == {str(p.resolve()): digest(p) for p in inputs}
		assert not list(out.glob(".acc-link-*"))
		(out / "verification.json").write_text(json.dumps({"all_expected": True, "commands": len(rows),
			"input_hashes": before, "flags": flags, "input_bytes_unchanged": True,
			"actual_runtime_Bool_parameter_and_two_Acc_branches": True,
			"source_true_false_and_opposite_maps_match": True,
			"ordinary_native_closed_and_publication_refusals_retained": True,
			"Core_scope": "retained finite ascending oracle; descending reversal composition, not fresh source oracle",
			"scope": "separate explicit target runtime candidate ABI; no producer/general native/full61/adoption/cost"}, indent=2) + "\n")
	except BaseException:
		(out / "failed.json").write_text(json.dumps({"commands": len(rows), "input_hashes": before}, indent=2) + "\n")
		raise


if __name__ == "__main__":
	main()
