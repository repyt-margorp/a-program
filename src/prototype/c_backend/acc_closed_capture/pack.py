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
		header = (stage / "component.h").read_bytes()
		if digest(source) != "59224615702be1b9413d8328eb21eac2dbaaf183849e061f931f702d370820b3":
			raise ValueError("unqualified actual Acc closed-capture parent")
		capture = (bodies / "capture.inc").read_bytes()
		# The finite source reader emits only these two operand permutations.
		# This qualifies its output, not a fallback when a source form refuses.
		template = ("/* Generated closed comparator capture: actual admitted known Nat comparison.\n"
			"\t* Operand order follows the source lambda; Acc/partition bodies are unchanged. */\n"
			"static int gclosed_compare(struct qs_arena *a, const void *context, uint32_t left, uint32_t right)\n"
			"{\n\t(void)context; return gc_compare(a,NULL,%s,%s);\n}\n")
		plain = [((template % pair).encode()) for pair in (("left", "right"), ("right", "left"))]
		boolean_template = ("/* Generated actual closed Bool capture and source matcher clauses.\n"
			"\t* Constructor positions and both Nat operand orders come from admitted views. */\n"
			"struct gclosed_context { unsigned boolean_tag; };\n"
			"static const struct gclosed_context gclosed_capture = {%d};\n"
			"static int gclosed_compare(struct qs_arena *a, const void *context, uint32_t left, uint32_t right)\n"
			"{\n\tconst struct gclosed_context *capture=context;\n"
			"\tif (!index_check(a,capture && capture->boolean_tag<2)) return 0;\n"
			"\tif (capture->boolean_tag==0) return gc_compare(a,NULL,%s,%s);\n"
			"\treturn gc_compare(a,NULL,%s,%s);\n}\n")
		boolean = [(boolean_template % (tag, *first, *second)).encode() for tag in (0, 1)
			for first in (("left", "right"), ("right", "left"))
			for second in (("left", "right"), ("right", "left"))]
		if capture not in plain + boolean:
			raise ValueError("unqualified source comparator capture")
		receipt = json.loads((stage / "provenance.json").read_text())
		boundary = next(s for s in receipt["sections"] if s["name"] == "array boundary")
		start = boundary["offset"]
		old_boundary = source[start:]
		if len(old_boundary) != boundary["bytes"] or digest(old_boundary) != boundary["sha256"] \
			or old_boundary.count(b"(struct qs_compare){gc_compare,NULL}") != 1:
			raise ValueError("unqualified manual array boundary")
		context = b"&gclosed_capture" if capture in boolean else b"NULL"
		new_boundary = old_boundary.replace(b"(struct qs_compare){gc_compare,NULL}", b"(struct qs_compare){gclosed_compare," + context + b"}")
		combined = source[:start] + capture + b"\n" + new_boundary
		boundary.update(offset=start + len(capture) + 1, bytes=len(new_boundary), sha256=digest(new_boundary))
		receipt["sections"].insert(-1, {"name": "closed comparator capture",
			"extent": "actual admitted closed source operand permutation; no substitute algorithm",
			"offset": start, "bytes": len(capture), "sha256": digest(capture)})
		for section in receipt["sections"]:
			assert digest(combined[section["offset"]:section["offset"] + section["bytes"]]) == section["sha256"]
		receipt["scope"] = "fixed actual Acc closed-source comparator capture candidate; not generalized native lowering"
		receipt["parent_component_sha256"] = digest(source)
		receipt["source_body_inputs"]["capture.inc"] = digest(capture)
		receipt["component.c"] = digest(combined)
		receipt["component.h"] = digest(header)
		if capture in boolean:
			receipt["scope"] = "fixed actual Acc closed Bool source capture candidate; not generalized native lowering"
			receipt["sections"][-2]["extent"] = "actual admitted Bool field/constructor/matcher and Nat operands; both source cases retained"
		output.mkdir()
		(output / "component.c").write_bytes(combined)
		(output / "component.h").write_bytes(header)
		(output / "provenance.json").write_text(json.dumps(receipt, indent=2) + "\n")
