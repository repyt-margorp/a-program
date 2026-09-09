#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" context-scopes
"$1" write "$directory/modules.graph"
single=$("$1" read "$directory/modules.graph")
bulk=$("$1" read-bulk "$directory/modules.graph")
test "$single" = "$bulk"
printf '%s\n' "$single"
for fixture in lambda nominal nullary application constructor match; do
	"$1" retained-write-typed "$directory/typed.a" "$fixture"
	"$1" retained-resave "$directory/typed.a" "$directory/typed-again.a"
	"$1" retained-resave "$directory/typed-again.a" "$directory/typed-final.a"
	"$1" retained-check "$directory/typed-final.a"
	"$1" retained-recompute "$directory/typed-final.a"
done
printf '%s\n' 'retained typed roots: ordinary proof checking preserves constructor and recursive Match NF inputs'
for fixture in lambda nominal nullary application constructor match; do
	"$1" retained-write "$directory/retained.a" "$fixture"
	"$1" retained-resave "$directory/retained.a" "$directory/resaved.a"
	"$1" retained-resave "$directory/resaved.a" "$directory/resaved-again.a"
	"$1" retained-check "$directory/resaved-again.a"
	"$1" retained-recompute "$directory/resaved-again.a"
done
printf '%s\n' 'source retained NF: independent writer/resavers/readers preserve source origins and checked reuse'
"$1" annotation-write "$directory/annotations.graph"
single=$("$1" annotation-read "$directory/annotations.graph")
bulk=$("$1" annotation-read-bulk "$directory/annotations.graph")
test "$single" = "$bulk"
printf '%s\n' "$single"
for writer in module-annotation-write module-annotation-write-settled; do
	"$1" "$writer" "$directory/module-annotations.graph"
	single=$("$1" module-annotation-read "$directory/module-annotations.graph")
	bulk=$("$1" module-annotation-read-bulk "$directory/module-annotations.graph")
	test "$single" = "$bulk"
	printf '%s\n' "$single"
done
"$1" nominal-write "$directory/nominal.graph"
single=$("$1" nominal-read "$directory/nominal.graph")
bulk=$("$1" nominal-read-bulk "$directory/nominal.graph")
test "$single" = "$bulk"
printf '%s\n' "$single"
