#!/usr/bin/env python3
"""Port the pinned Bend trees obligations; helper/proof encodings differ."""

import argparse
import hashlib
import json
from pathlib import Path
import re
from fetch_trees import HASHES, REVISION


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("bend_source", type=Path)
	parser.add_argument("output", type=Path)
	parser.add_argument("--count", type=int, default=400)
	parser.add_argument("--false", choices=("bool", "tree"))
	args = parser.parse_args()
	source = args.bend_source.read_text()
	if hashlib.sha256(args.bend_source.read_bytes()).hexdigest() != HASHES["bend"]:
		parser.error("source hash differs from the pinned official workload")
	ft = re.findall(r"law ft(\d+):\s+\{alltrue\(full\(n(\d+)\(\)\)\) == T\{\} : Bool\}", source)
	mt = re.findall(r"law mt(\d+):\s+\{mirror\(full\(n(\d+)\(\)\)\) == full\(n(\d+)\(\)\) : Tree\}", source)
	if len(ft) != 400 or len(mt) != 400 or not 0 <= args.count <= 400:
		parser.error("expected the official 400-pair trees workload")
	if args.false and not args.count:
		parser.error("a false control requires at least one pair")
	if any(int(i) != j for j, (i, _) in enumerate(ft)) or any(
		int(i) != j or left != right for j, (i, left, right) in enumerate(mt)):
		parser.error("noncanonical obligation sequence")
	lines = [
		"// Concrete computational equalities, not universal tree theorems.",
		"Nat := @{zero:*; succ:*->*;};",
		"Bool := @{true:*; false:*;};",
		"Tree := @{leaf:*; node:*->*->*;};",
		"Eq := \\A:@ => @\\left:A => @\\right:A => {refl:(x:A)->* x x;};",
		"and := \\a:Bool => \\b:Bool => a @true => b @false => Bool.false;",
		"full := \\n:Nat => n @zero => Tree.leaf @succ p => Tree.node *p *p;",
		"full :: Nat->Tree;",
		"mirror := \\t:Tree => t @leaf => Tree.leaf @node l r => Tree.node *r *l;",
		"mirror :: Tree->Tree;",
		"alltrue := \\t:Tree => t @leaf => Bool.true @node l r => and *l *r;",
		"alltrue :: Tree->Bool;",
		"n0 := Nat.zero;",
	]
	lines.extend(f"n{i} := Nat.succ n{i - 1};" for i in range(1, 14))
	for i in range(args.count):
		df, dm = ft[i][1], mt[i][1]
		boolean = "Bool.false" if i == 0 and args.false == "bool" else "Bool.true"
		tree = "Tree.leaf" if i == 0 and args.false == "tree" else f"full n{dm}"
		lines.extend((
			f"ft{i} := (Eq Bool).refl {boolean};",
			f"ft{i} :: Eq Bool (alltrue (full n{df})) {boolean};",
			f"mt{i} := (Eq Tree).refl ({tree});",
			f"mt{i} :: Eq Tree (mirror (full n{dm})) ({tree});",
		))
	args.output.parent.mkdir(parents=True, exist_ok=True)
	args.output.write_text("\n".join(lines) + "\n")
	metadata = {"bend_revision": REVISION, "pairs": args.count, "equalities": 2 * args.count,
		"false_control": args.false, "bend_source_sha256": hashlib.sha256(
			args.bend_source.read_bytes()).hexdigest(),
		"ap_source_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
		"depth_pairs": [[int(ft[i][1]), int(mt[i][1])] for i in range(args.count)]}
	args.output.with_suffix(".json").write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
	main()
