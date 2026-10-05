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
		data = (stage / "component.c").read_bytes()
		if digest(data) != "59224615702be1b9413d8328eb21eac2dbaaf183849e061f931f702d370820b3":
			raise ValueError("unqualified actual Acc comparator parent")
		runtime = (here / "runtime.c").read_bytes()
		header = (here / "module.h").read_bytes()
		combined = data + b"\n" + runtime
		receipt = json.loads((stage / "provenance.json").read_text())
		receipt["scope"] = "fixed actual Acc comparator-parameter candidate; not generalized native lowering"
		receipt["comparator_parameter"] = "admitted quoted pure-total Nat/Nat/Bool; borrowed C code/context interpretation"
		receipt["parent_component_sha256"] = digest(data)
		receipt["sections"].append({"name": "borrowed comparator array bridge",
			"extent": "manual target staging/code/context; actual algorithm bodies retained",
			"offset": len(data) + 1, "bytes": len(runtime), "sha256": digest(runtime)})
		receipt["component.c"] = digest(combined)
		receipt["component.h"] = digest(header)
		output.mkdir()
		(output / "component.c").write_bytes(combined)
		(output / "component.h").write_bytes(header)
		(output / "provenance.json").write_text(json.dumps(receipt, indent=2) + "\n")
