#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
root=$(dirname "${BASH_SOURCE[0]}")
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
provider="$root/fixtures/sorted-proof-provider.p"
proof="$root/acceptance/sort-insertion-property.p"
expected=b859e517843b40a9448e26e28ab9d26f926e4485d6e1f62a6ef763d95aa3639c
actual=$(sha256sum "$provider")
test "${actual%% *}" = "$expected"

# Preserve the exact historical provider, including its legacy #. spelling.
"$binary" --legacy-intrinsic-dot --steps 1000000 --imports "$provider" --save "$directory/proof.a" "$proof"
for pair in main:three duplicate:three after:three empty:one packet_value:expected_value direct_value:expected_value; do
	"$compare" --equal-image "$directory/proof.a" "${pair%:*}" "${pair#*:}"
done
for steps in 0 100; do
	code=0
	"$binary" --legacy-intrinsic-dot --imports "$provider" --steps "$steps" --save "$directory/partial.a" "$proof" || code=$?
	test "$code" = 3
	code=0
	"$binary" --load --steps 0 --save "$directory/resaved.a" "$directory/partial.a" || code=$?
	test "$code" = 3
	cmp "$directory/partial.a" "$directory/resaved.a"
	"$compare" --equal-image "$directory/resaved.a" main three
done

# Assemble the same definitions without imports as a single exported module
# for the negative consumer; no alternate algorithm or order axiom is added.
cat "$provider" > "$directory/provider.p"
sed '/^import /d' "$proof" >> "$directory/provider.p"
code=0
"$binary" --legacy-intrinsic-dot --steps 1000000 --imports "$directory/provider.p" "$root/acceptance/sort-insertion-property-wrong.p" || code=$?
test "$code" = 1
printf '%s\n' 'sort insertion: exact provider, universal property, execution witnesses, images and wrong result index passed'
