import hashlib
import json
import pathlib
import runpy
import sys
import tempfile


def digest(data):
	return hashlib.sha256(data).hexdigest()


def package(products, descriptor, output):
	here = pathlib.Path(__file__).resolve().parent
	# Updated only after the current readonly emitter supplies the pinned table.
	expected_descriptor = "14876c352a9be2f61e1d907bd0f82a35bc44257e626d01b65390cc6d0c2a6062"
	table = descriptor.read_bytes()
	if digest(table) != expected_descriptor:
		raise ValueError("unqualified actual-source transport descriptor")
	old_pack = runpy.run_path(str(here.parent / "acc_products/pack.py"))
	with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
		original = pathlib.Path(temporary) / "original"
		old_pack["package"](products, original)
		old_source = (original / "component.c").read_bytes()
		receipt = json.loads((original / "provenance.json").read_text())
		header = (original / "component.h").read_bytes()
	old_runtime = (here.parent / "acc_actions/runtime.c").read_bytes()
	new_runtime_input = (here / "runtime.c").read_bytes()
	if digest(old_source) != "49d3a0797ba60fbf83d0dd53bef9262e219f194387c68f293419b0243d8619ee" or old_source.count(old_runtime) != 1:
		raise ValueError("sealed actual Acc composition changed")
	marker = b'#include "transport.inc"\n'
	if new_runtime_input.count(marker) != 1:
		raise ValueError("transport descriptor insertion changed")
	new_runtime = new_runtime_input.replace(marker, table)
	start = old_source.index(old_runtime)
	source = old_source.replace(old_runtime, new_runtime)
	difference = len(new_runtime) - len(old_runtime)
	for section in receipt["sections"]:
		if section["name"] == "scoped actions":
			assert section["offset"] == start
			section.update(bytes=len(new_runtime), sha256=digest(new_runtime),
				extent="source-derived ordered map vectors and nominal endpoint projections; manual bounded action interpretation")
		elif section["offset"] > start:
			section["offset"] += difference
		assert digest(source[section["offset"]:section["offset"] + section["bytes"]]) == section["sha256"]
	receipt.update(scope="C48 fixed actual Acc endpoint-projection candidate; not general native lowering",
		transport_descriptor=digest(table), target_runtime_input=digest(new_runtime_input),
		sealed_C45_component=digest(old_source))
	receipt["component.c"] = digest(source)
	output.mkdir()
	(output / "component.c").write_bytes(source)
	(output / "component.h").write_bytes(header)
	(output / "provenance.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
	if len(sys.argv) != 4:
		sys.exit("usage: python3 pack.py EXACT_C44_PRODUCTS QUALIFIED_DESCRIPTOR NEW_OUTPUT")
	try:
		package(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), pathlib.Path(sys.argv[3]))
	except ValueError as error:
		print(str(error), file=sys.stderr)
		sys.exit(4)
	except OSError as error:
		print(str(error), file=sys.stderr)
		sys.exit(2)
