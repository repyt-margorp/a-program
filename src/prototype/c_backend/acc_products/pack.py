import hashlib
import json
import pathlib
import sys


def digest(data):
	return hashlib.sha256(data).hexdigest()


def read_exact(path, expected):
	data = path.read_bytes()
	if digest(data) != expected:
		raise ValueError("unqualified input: " + str(path))
	return data.decode("ascii")


def section(text, start, stop=None):
	if text.count(start) != 1 or (stop and text.count(stop) != 1):
		raise ValueError("fixed candidate section changed")
	return text[text.index(start):text.index(stop) if stop else len(text)]


def package(products, output):
	root = pathlib.Path(__file__).resolve().parent.parent
	manual = {
		"acc_quicksort_mockup/mockup.h": "0fdaeb6687be5b51ce865caf1698619f99d16c1c0cffc3f38a80fca0967ed0aa",
		"acc_quicksort_mockup/mockup.c": "762f264ccad935d32a5fd2ad41958bc9cbd6882c98b829b8dae0ccacb7041cfe",
		"acc_actions/runtime.c": "386af9b1f04b714a32ca86668298e4e0f2c922a2e8850cc2d1a5dbcb9502c923",
		"acc_capture/support.c": "f04a4095824c283e3f637688eace944600b079116e215122bee18e9ba2ad5e41",
	}
	generated = {
		"actions.inc": "1688e9e3618e8c89edef8e5286180df715b67aaec7fa932e0b4525704c129180",
		"accessibility.inc": "ccdd139cc3e9414a398fb12bb7073a652d91d47f9cb37f84c3319d86310f55b9",
		"comparison.inc": "6b63a59d024eec5a46f7c436ed300ecab72283e7a5e10fc565e23ffef2ba69c4",
		"partition.inc": "b8238b022d1613d4cd8dffd4692f68d0e6668bf8206f3324588765bc25c2de31",
		"append.inc": "ffd37ec2ed3ff967b588757b430b3a83042b4bfd737414141a0d6dffffefa6ac",
		"clause.inc": "cd1a7d70441222995b12ca2354ae6aeda80eaa2f99cd296cfeb5ef451405804c",
		"measure.inc": "60d51b65f7cedf55ea25d5e44251695ffb7adc98b35445bf63e1547237755803",
	}
	inputs = {name: read_exact(root / name, sha) for name, sha in manual.items()}
	bodies = {name: read_exact(products / name, sha) for name, sha in generated.items()}
	header = (root / "acc_products/module.h").read_bytes()
	parts = []
	def add(label, authority, text):
		parts.append({"name": label, "extent": authority, "text": text})
	add("preamble", "target packaging", '#include "component.h"\n#include <stdlib.h>\n')
	add("private representations", "manual target layout",
		section(inputs["acc_quicksort_mockup/mockup.h"], "struct qs_arena;", "/* Source-definition correspondence;"))
	add("storage and Nat/LT primitives", "manual target primitives; zero-down foreign refusal",
		section(inputs["acc_quicksort_mockup/mockup.c"], "struct qs_nat_type {", "/* accessibleSucc's down closure"))
	add("scoped actions", "manual bounded action/closure representation", inputs["acc_actions/runtime.c"])
	add("successor down and captures", "actual source expression emission C43/C44", bodies["actions.inc"])
	add("successor adapter", "target composition", "#define qs_accessible_succ(...) gs_accessible_succ(__VA_ARGS__)\n")
	add("Nat accessibility", "actual source expression emission C39", bodies["accessibility.inc"])
	add("successor adapter end", "target composition", "#undef qs_accessible_succ\n")
	for name, epoch in [("comparison", "C41"), ("partition", "C37"), ("append", "C38")]:
		add(name, "actual source expression emission " + epoch, bodies[name + ".inc"])
	add("partition/append adapters", "target composition", "#define qs_partition(...) gp_partition(__VA_ARGS__)\n#define append(...) ga_append(__VA_ARGS__)\n")
	add("Acc QuickSort callable Fold", "actual source expression emission C36", bodies["clause.inc"])
	add("adapters end", "target composition", "#undef append\n#undef qs_partition\n")
	add("measure and outer QuickSort", "actual source expression emission C40", bodies["measure.inc"])
	add("array boundary", "manual target staging/copy-out/lifetime", section(inputs["acc_capture/support.c"], "/* Target array staging/copy-out/arena lifetime;"))
	source = "/* Fixed actual Acc candidate: source bodies and manual target extent are\n\t* labeled below. Historical body comments describe their original epoch.\n\t* This packager does not perform source admission or general lowering. */\n"
	sections = []
	for part in parts:
		source += "\n/* " + part["name"] + ": " + part["extent"] + ". */\n"
		start = len(source.encode("ascii"))
		source += part["text"]
		sections.append({"name": part["name"], "extent": part["extent"], "offset": start,
			"bytes": len(part["text"].encode("ascii")), "sha256": digest(part["text"].encode("ascii"))})
	data = source.encode("ascii")
	receipt = {"scope": "fixed C44 actual-source candidate packaging; not general backend success",
		"manual_inputs": manual, "source_body_inputs": generated, "sections": sections,
		"component.c": digest(data), "component.h": digest(header)}
	output.mkdir()
	(output / "component.c").write_bytes(data)
	(output / "component.h").write_bytes(header)
	(output / "provenance.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
	if len(sys.argv) != 3:
		sys.exit("usage: python3 pack.py EXACT_C44_PRODUCTS NEW_OUTPUT_DIRECTORY")
	try:
		package(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]))
	except ValueError as error:
		print(str(error), file=sys.stderr)
		sys.exit(4)
	except OSError as error:
		print(str(error), file=sys.stderr)
		sys.exit(2)
