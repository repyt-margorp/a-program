import hashlib
import json
import pathlib
import sys


def digest(data):
	return hashlib.sha256(data).hexdigest()


def package(parent, descriptor, output):
	here = pathlib.Path(__file__).resolve().parent
	source = (parent / "component.c").read_bytes()
	header = (parent / "component.h").read_bytes()
	provenance = (parent / "provenance.json").read_bytes()
	if digest(source) != "53f01aa8932efcd57f9395183d77402fd6779cb29f49a04dc2260a5dfb87089d" \
		or digest(header) != "246364a170ff3443c5e89a15e033ecaf3e230ea657ee8a670b0e4582cf78c044" \
		or digest(provenance) != "79ac358c50b19ca9213dae1cb905f1372f1f8e4cdf50d4d0d5e65739bba947a5":
		raise ValueError("unqualified actual Acc parent component")
	table = descriptor.read_bytes()
	if digest(table) != "591fa73f5abcd1b5257e7ee3c271672452fa63c93c9b888aa9d1063445c8e6d0":
		raise ValueError("unqualified actual Acc down/action recipe")
	old_table = table.split(b"/* Actual admitted Acc constructor/down classifier:")[0]
	if digest(old_table) != "888dbff17e481d65fb071612fce4f05b24853843623a804db76c6f918e3ec0de":
		raise ValueError("sealed source map/endpoint/frame prefix changed")
	marker = b'#include "transport.inc"\n'
	old_runtime_input = (here.parent / "acc_frame/runtime.c").read_bytes()
	new_runtime_input = (here / "runtime.c").read_bytes()
	if digest(old_runtime_input) != "b325609ac5b4e5a03bfe1b539b5733748c9a14ee89e7e2381c40b62e7c9c2a01" \
		or old_runtime_input.count(marker) != 1 or new_runtime_input.count(marker) != 1:
		raise ValueError("runtime/recipe insertion contract changed")
	old_runtime = old_runtime_input.replace(marker, old_table)
	new_runtime = new_runtime_input.replace(marker, table)
	if source.count(old_runtime) != 1:
		raise ValueError("sealed scoped action section changed")
	start = source.index(old_runtime)
	new_source = source.replace(old_runtime, new_runtime)
	difference = len(new_runtime) - len(old_runtime)
	receipt = json.loads(provenance)
	for section in receipt["sections"]:
		if section["name"] == "scoped actions":
			assert section["offset"] == start
			section.update(bytes=len(new_runtime), sha256=digest(new_runtime),
				extent="source-derived maps/endpoints/frame/down-domain/result/direction recipe; finite manual target representation")
		elif section["offset"] > start:
			section["offset"] += difference
		assert digest(new_source[section["offset"]:section["offset"] + section["bytes"]]) == section["sha256"]
	receipt.update(scope="C51 fixed actual Acc down/action recipe; not general native lowering",
		transport_descriptor=digest(table), target_runtime_input=digest(new_runtime_input),
		sealed_C49_component=digest(source))
	receipt["component.c"] = digest(new_source)
	output.mkdir()
	(output / "component.c").write_bytes(new_source)
	(output / "component.h").write_bytes(header)
	(output / "provenance.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
	if len(sys.argv) != 4:
		sys.exit("usage: python3 pack.py EXACT_C49_OR_C50_PRODUCT QUALIFIED_RECIPE NEW_OUTPUT")
	try:
		package(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), pathlib.Path(sys.argv[3]))
	except ValueError as error:
		print(str(error), file=sys.stderr)
		sys.exit(4)
	except OSError as error:
		print(str(error), file=sys.stderr)
		sys.exit(2)
