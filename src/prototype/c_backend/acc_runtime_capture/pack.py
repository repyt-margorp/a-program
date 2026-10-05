import hashlib
import json
import pathlib
import runpy
import tempfile


def digest(data):
	return hashlib.sha256(data).hexdigest()


def package(bodies, recipe, output):
	here = pathlib.Path(__file__).resolve().parent
	parent = runpy.run_path(str(here.parent / "acc_create_command/recipe_pack.py"))["package"]
	with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
		stage = pathlib.Path(temporary) / "parent"
		parent(bodies, recipe, stage)
		source = (stage / "component.c").read_bytes()
		if digest(source) != "59224615702be1b9413d8328eb21eac2dbaaf183849e061f931f702d370820b3":
			raise ValueError("unqualified actual Acc runtime-capture parent")
		capture = (bodies / "capture.inc").read_bytes()
		template = ("/* Generated actual runtime source Bool capture and both matcher clauses.\n"
			"\t* The field comes from the admitted outer parameter, not a foreign callback. */\n"
			"struct gruntime_context { unsigned boolean_tag; };\n"
			"static int gruntime_compare(struct qs_arena *a, const void *context, uint32_t left, uint32_t right)\n"
			"{\n\tconst struct gruntime_context *capture=context;\n"
			"\tif (!index_check(a,capture && capture->boolean_tag<2)) return 0;\n"
			"\tif (capture->boolean_tag==0) return gc_compare(a,NULL,%s,%s);\n"
			"\treturn gc_compare(a,NULL,%s,%s);\n}\n")
		choices = [(template % (*first, *second)).encode() for first in (("left", "right"), ("right", "left"))
			for second in (("left", "right"), ("right", "left"))]
		if capture not in choices:
			raise ValueError("unqualified admitted runtime Bool source capture")
		header = (here / "module.h").read_bytes()
		runtime = (here / "runtime.c").read_bytes()
		receipt = json.loads((stage / "provenance.json").read_text())
		boundary = receipt["sections"][-1]
		if boundary["name"] != "array boundary" or boundary["offset"] + boundary["bytes"] != len(source):
			raise ValueError("qualified array boundary extent changed")
		start = boundary["offset"]
		combined = source[:start] + capture + b"\n" + runtime
		receipt["sections"].insert(-1, {"name": "runtime source Bool capture",
			"extent": "admitted outer parameter/environment/matcher and both Nat operand clauses",
			"offset": start, "bytes": len(capture), "sha256": digest(capture)})
		boundary.update(offset=start + len(capture) + 1, bytes=len(runtime), sha256=digest(runtime),
			extent="manual synchronous source-parameter/array/lifetime ABI bridge")
		for section in receipt["sections"]:
			assert digest(combined[section["offset"]:section["offset"] + section["bytes"]]) == section["sha256"]
		receipt.update(scope="fixed actual Acc runtime Bool source-parameter candidate; not general native lowering",
			abi="c_acc_runtime_bool_candidate_v1", parent_component_sha256=digest(source),
			target_runtime_input=digest(runtime))
		receipt["source_body_inputs"]["capture.inc"] = digest(capture)
		receipt["component.c"] = digest(combined)
		receipt["component.h"] = digest(header)
		output.mkdir()
		(output / "component.c").write_bytes(combined)
		(output / "component.h").write_bytes(header)
		(output / "provenance.json").write_text(json.dumps(receipt, indent=2) + "\n")
