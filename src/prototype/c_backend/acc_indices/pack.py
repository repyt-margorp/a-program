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
	if digest(source) != "1da176b52c9dabb6a53e244dd77fb4172221d46b13052e231e32d536cf834f3d" \
		or digest(header) != "246364a170ff3443c5e89a15e033ecaf3e230ea657ee8a670b0e4582cf78c044" \
		or digest(provenance) != "f3cf55d5a9eb6905d559109bda20beb49966dd627de7cc48925fe8eb91894de2":
		raise ValueError("unqualified actual Acc C51/C52 parent component")
	table = descriptor.read_bytes()
	if digest(table) != "96dd3c3e016cdf2594d02b97a6d6f07e09a2e46561f4e0a7c32b69e2211ff173":
		raise ValueError("unqualified actual LT constructor index recipe")
	prefix = table.split(b"/* Actual admitted LT constructor field classifiers")[0]
	if digest(prefix) != "591fa73f5abcd1b5257e7ee3c271672452fa63c93c9b888aa9d1063445c8e6d0":
		raise ValueError("sealed source map/endpoint/frame/down prefix changed")
	marker = b'#include "transport.inc"\n'
	old_runtime_input = (here.parent / "acc_recipe/runtime.c").read_bytes()
	new_runtime_input = (here / "runtime.c").read_bytes()
	if old_runtime_input.count(marker) != 1 or new_runtime_input.count(marker) != 1:
		raise ValueError("runtime/index insertion contract changed")
	old_runtime = old_runtime_input.replace(marker, prefix)
	new_runtime = new_runtime_input.replace(marker, table)
	if source.count(old_runtime) != 1:
		raise ValueError("sealed actual Acc scoped action section changed")
	start = source.index(old_runtime)
	new_source = source.replace(old_runtime, new_runtime)
	difference = len(new_runtime) - len(old_runtime)
	receipt = json.loads(provenance)
	for section in receipt["sections"]:
		if section["name"] == "scoped actions":
			assert section["offset"] == start
			section.update(bytes=len(new_runtime), sha256=digest(new_runtime),
				extent="source-derived maps/endpoints/frame/down/directions/LT constructor indices; finite manual target storage")
		elif section["offset"] > start:
			section["offset"] += difference
		assert digest(new_source[section["offset"]:section["offset"] + section["bytes"]]) == section["sha256"]
	receipt.update(scope="C53 fixed actual Acc/LT constructor index candidate; not general native lowering",
		transport_descriptor=digest(table), target_runtime_input=digest(new_runtime_input),
		sealed_C51_component=digest(source))
	receipt["component.c"] = digest(new_source)
	output.mkdir()
	(output / "component.c").write_bytes(new_source)
	(output / "component.h").write_bytes(header)
	(output / "provenance.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
	if len(sys.argv) != 4:
		sys.exit("usage: python3 pack.py EXACT_C51_OR_C52_PRODUCT QUALIFIED_INDEX_RECIPE NEW_OUTPUT")
	try:
		package(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), pathlib.Path(sys.argv[3]))
	except ValueError as error:
		print(str(error), file=sys.stderr)
		sys.exit(4)
	except OSError as error:
		print(str(error), file=sys.stderr)
		sys.exit(2)
