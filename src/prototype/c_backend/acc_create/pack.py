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
	if digest(source) != "b3cfdfe9bb7e61c1b1b98e4c5315044215e8fbce90ce3be594b371a7fc6df557" \
		or digest(header) != "246364a170ff3443c5e89a15e033ecaf3e230ea657ee8a670b0e4582cf78c044" \
		or digest(provenance) != "d44dd61ee9b9374c00718043477abbca01cf0dee6a945c6c7ac1e36a8314b79f":
		raise ValueError("unqualified actual Acc C53 parent component")
	table = descriptor.read_bytes()
	if digest(table) != "f8954c46bf74754207074cb92c16c3cc8eb5207d99cf6a46920390181300eac8":
		raise ValueError("unqualified actual LT constructor creation recipe")
	prefix = table[:2913]
	if digest(prefix) != "96dd3c3e016cdf2594d02b97a6d6f07e09a2e46561f4e0a7c32b69e2211ff173":
		raise ValueError("sealed source index/map/endpoint/frame/down prefix changed")
	marker = b'#include "transport.inc"\n'
	old_runtime_input = (here.parent / "acc_indices/runtime.c").read_bytes()
	new_runtime_input = (here / "runtime.c").read_bytes()
	if old_runtime_input.count(marker) != 1 or new_runtime_input.count(marker) != 1:
		raise ValueError("runtime/creation insertion contract changed")
	receipt = json.loads(provenance)
	sections = {s["name"]: s for s in receipt["sections"]}
	storage = sections["storage and Nat/LT primitives"]
	old_storage = source[storage["offset"]:storage["offset"] + storage["bytes"]]
	start = old_storage.index(b"/* The three actual LT constructors")
	end = old_storage.index(b"static const struct qs_acc *call_raw_down", start)
	declarations = b"/* Concrete LT creation below uses admitted prior/result index recipes. */\n" \
		b"static const struct qs_lt *lt_step(struct qs_arena *, uint32_t);\n" \
		b"static const struct qs_lt *lt_weaken(struct qs_arena *, uint32_t, uint32_t, const struct qs_lt *);\n" \
		b"static const struct qs_lt *lt_lift(struct qs_arena *, uint32_t, uint32_t, const struct qs_lt *);\n\n"
	replacements = {
		"storage and Nat/LT primitives": old_storage[:start] + declarations + old_storage[end:],
		"scoped actions": new_runtime_input.replace(marker, table),
	}
	actions = sections["scoped actions"]
	old_actions = old_runtime_input.replace(marker, prefix)
	if source[actions["offset"]:actions["offset"] + actions["bytes"]] != old_actions:
		raise ValueError("sealed actual Acc action section changed")
	new_source = source
	for section in reversed(receipt["sections"]):
		start, end = section["offset"], section["offset"] + section["bytes"]
		if digest(source[start:end]) != section["sha256"]:
			raise ValueError("parent section provenance changed")
		if section["name"] in replacements:
			new_source = new_source[:start] + replacements[section["name"]] + new_source[end:]
	shift = 0
	for section in receipt["sections"]:
		section["offset"] += shift
		if section["name"] in replacements:
			data = replacements[section["name"]]
			shift += len(data) - section["bytes"]
			section.update(bytes=len(data), sha256=digest(data))
		assert digest(new_source[section["offset"]:section["offset"] + section["bytes"]]) == section["sha256"]
	storage["extent"] = "manual storage/Nat primitives; source-selected LT creation below"
	actions["extent"] = "source-derived Acc actions/LT prior-result recipes; finite manual storage/roles"
	receipt.update(scope="C54 fixed actual Acc/LT constructor creation candidate; not general native lowering",
		transport_descriptor=digest(table), target_runtime_input=digest(new_runtime_input),
		sealed_C53_component=digest(source))
	receipt["component.c"] = digest(new_source)
	output.mkdir()
	(output / "component.c").write_bytes(new_source)
	(output / "component.h").write_bytes(header)
	(output / "provenance.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
	if len(sys.argv) != 4:
		sys.exit("usage: python3 pack.py EXACT_C53_PRODUCT QUALIFIED_CREATION_RECIPE NEW_OUTPUT")
	try:
		package(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), pathlib.Path(sys.argv[3]))
	except ValueError as error:
		print(str(error), file=sys.stderr)
		sys.exit(4)
	except OSError as error:
		print(str(error), file=sys.stderr)
		sys.exit(2)
