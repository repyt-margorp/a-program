#!/usr/bin/env bash
set -euo pipefail
binary=$1
root=$(dirname "${BASH_SOURCE[0]}")
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT

# Keep the frozen provider and universal theorem as the comparison inputs.
# No fuzzy matching: source changes must trigger review of this experiment.
cp "$root/fixtures/sorted-proof-provider.p" "$directory/provider.p"
cp "$root/acceptance/generic-quick-sorted.p" "$directory/proof.p"
patch --batch --silent --fuzz=0 -p1 -d "$directory" < "$root/fixtures/generic_sorted/derived-lt.patch"

for mode in ordinary retained; do
	options=()
	if [[ $mode == retained ]]; then options+=(--retain-reductions); fi
	"$binary" --steps 10000000 --legacy-intrinsic-dot --imports "$directory/provider.p" \
		--save "$directory/complete.a" "${options[@]}" "$directory/proof.p"
	"$binary" --steps 10000000 --load "$directory/complete.a"
	code=0
	"$binary" --load --steps 0 --save "$directory/resaved.a" "${options[@]}" \
		"$directory/complete.a" > "$directory/status" || code=$?
	[[ $code == 3 ]] && grep -qx 'pending steps=0' "$directory/status"
	cmp "$directory/complete.a" "$directory/resaved.a"
done
printf '%s\n' 'derived LT: parallel provider, complete universal Sorted and ordinary/retained images passed'
