#!/usr/bin/env python3
"""Prepare focused cross-checker controls from hash-pinned official sources."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "performance"))
from fetch_trees import HASHES, REVISION


def replace_once(source, before, after):
	if source.count(before) != 1:
		raise ValueError(f"expected one occurrence of {before!r}")
	return source.replace(before, after, 1)


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("official_sources", type=Path)
	parser.add_argument("output", type=Path)
	args = parser.parse_args()
	args.output.mkdir(parents=True, exist_ok=False)
	controls = {
		"bend": ("law ft2:", "", "{alltrue(full(n7())) == T{} : Bool}",
			"{alltrue(full(n7())) == F{} : Bool}",
			"{mirror(full(n6())) == full(n6()) : Tree}",
			"{mirror(full(n6())) == L{} : Tree}"),
		"lean": ("theorem ft2 :", "end Bench\n", "alltrue (full n7) = .T := rfl",
			"alltrue (full n7) = .F := rfl", "mirror (full n6) = full n6 := rfl",
			"mirror (full n6) = .L := rfl"),
		"agda": ("ft2 :", "", "ft0 : Eq (alltrue (full n7)) T",
			"ft0 : Eq (alltrue (full n7)) F", "mt0 : Eq (mirror (full n6)) (full n6)",
			"mt0 : Eq (mirror (full n6)) L"),
		"v": ("Lemma ft2 :", "", "Lemma ft0 : alltrue (full n7) = T.",
			"Lemma ft0 : alltrue (full n7) = F.", "Lemma mt0 : mirror (full n6) = full n6.",
			"Lemma mt0 : mirror (full n6) = L."),
	}
	manifest = {"revision": REVISION, "equalities": 4, "depth_pairs": [[7, 6], [12, 9]],
		"qualification": "focused controls only; helpers/proof encodings differ",
		"comparative_measurements": False, "files": []}
	for extension, (cut, suffix, bool_before, bool_after, tree_before, tree_after) in controls.items():
		file = args.official_sources / f"main.{extension}"
		source = file.read_text()
		if hashlib.sha256(file.read_bytes()).hexdigest() != HASHES[extension]:
			raise ValueError(f"unexpected official source hash: {file}")
		positive = source[:source.index(cut)] + suffix
		# Count only the four concrete obligations, excluding helper declarations.
		pattern = r"^(?:law |theorem |Lemma )?(?:ft|mt)(\d+)\s*:"
		if re.findall(pattern, positive, re.MULTILINE) != ["0", "0", "1", "1"]:
			raise ValueError(f"unexpected focused obligation sequence: {extension}")
		cases = {"positive": positive,
			"false-bool": replace_once(positive, bool_before, bool_after),
			"false-tree": replace_once(positive, tree_before, tree_after)}
		for case, content in cases.items():
			folder = args.output / extension / case
			folder.mkdir(parents=True)
			output = folder / f"main.{extension}"
			output.write_text(content)
			manifest["files"].append({"extension": extension, "case": case,
				"path": str(output.relative_to(args.output)), "expected_pass": case == "positive",
				"sha256": hashlib.sha256(output.read_bytes()).hexdigest()})
	(args.output / "sources.json").write_text(json.dumps(manifest, indent=2) + "\n")
	print("Prepared 12 focused controls from four unchanged pinned sources.")


if __name__ == "__main__":
	main()
