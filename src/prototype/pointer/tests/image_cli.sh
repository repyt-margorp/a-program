#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
fixture=$1
binary=$2
"$fixture" nominal-write "$directory/multiple.a"
"$binary" --load --root 4 --save "$directory/saved.a" "$directory/multiple.a" > "$directory/status"
cmp "$directory/multiple.a" "$directory/saved.a"
# The selected root does not erase the separately rejected root or its inputs.
code=0
"$binary" --load --root 2 "$directory/saved.a" > "$directory/status" || code=$?
test "$code" = 1
grep -q '^rejected steps=' "$directory/status"
"$fixture" nominal-read "$directory/saved.a"
code=0
"$binary" --load --root 9 "$directory/multiple.a" > "$directory/status" 2>&1 || code=$?
test "$code" = 2
grep -q 'root index out of range: 9 (count 8)' "$directory/status"
example="$(dirname "${BASH_SOURCE[0]}")/../../../../examples/09_list_induction.p"
"$binary" --nf main --save "$directory/list.a" "$example" > "$directory/source-nf"
"$binary" --load --nf main "$directory/list.a" > "$directory/image-nf"
# Solve scheduling may differ; the fully reduced computation graph must not.
sed '1d' "$directory/source-nf" > "$directory/source-value"
sed '1d' "$directory/image-nf" > "$directory/image-value"
cmp "$directory/source-value" "$directory/image-value"

# A failed write must not truncate a previously usable image, even in place.
cp "$directory/list.a" "$directory/list-before.a"
code=0
(
	trap '' XFSZ
	ulimit -f 1
	"$binary" --load --save "$directory/list.a" "$directory/list.a"
) > "$directory/status" 2>&1 || code=$?
test "$code" = 2
grep -q 'cannot save input image' "$directory/status"
cmp "$directory/list-before.a" "$directory/list.a"
if compgen -G "$directory/list.a.tmp.*" > /dev/null; then exit 1; fi
"$binary" --load --save "$directory/list.a" "$directory/list.a" > "$directory/status"
"$binary" --load --nf main "$directory/list.a" > "$directory/image-nf"
sed '1d' "$directory/image-nf" > "$directory/image-value"
cmp "$directory/source-value" "$directory/image-value"
# Failure to publish (a directory target) also removes the temporary file.
mkdir "$directory/target"
code=0
"$binary" --load --save "$directory/target" "$directory/list.a" > "$directory/status" 2>&1 || code=$?
test "$code" = 2
test -d "$directory/target"
if compgen -G "$directory/target.tmp.*" > /dev/null; then exit 1; fi
printf '%s\n' 'image cli: failed writes preserve existing images; in-place publication and cleanup passed'
printf '%s\n' 'image cli: multi-root selection, retained obligations and range rejection passed'
printf '%s\n' 'image cli: parameterized List source/image NF agreement passed'

# Retaining syntax does not require its type-level computations to have finished.
# Compare admission with direct Solve without treating unsupported as accepted.
for family in closed-family open-family; do
	input="$(dirname "${BASH_SOURCE[0]}")/acceptance/$family.p"
	direct=0
	"$binary" "$input" > "$directory/direct" || direct=$?
	case "$direct" in 0|4) ;; *) exit 1 ;; esac
	for steps in 0 100; do
		code=0
		"$binary" --steps "$steps" --save "$directory/family.a" "$input" > "$directory/status" || code=$?
		test "$code" = 3
		grep -q '^pending steps=' "$directory/status"
		code=0
		"$binary" --load --steps 0 --save "$directory/family-resaved.a" "$directory/family.a" > "$directory/status" || code=$?
		test "$code" = 3
		grep -q '^pending steps=0$' "$directory/status"
		code=0
		"$binary" --load "$directory/family-resaved.a" > "$directory/restored" || code=$?
		test "$code" = "$direct"
		cut -d ' ' -f 1 "$directory/direct" > "$directory/direct-status"
		cut -d ' ' -f 1 "$directory/restored" > "$directory/restored-status"
		cmp "$directory/direct-status" "$directory/restored-status"
	done
	if test "$family" = closed-family; then test "$direct" = 0; fi
done
printf '%s\n' 'image cli: unfinished family inputs preserve direct Solve admission after unsolved resave'
