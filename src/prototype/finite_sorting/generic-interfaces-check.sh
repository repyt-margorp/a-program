#!/usr/bin/env bash
set -euo pipefail
binary=$1
steps=${SORTING_CHECK_STEPS:-40000000}
image_options=()
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
bash "$here/provider.sh" "$here/quick.p" "$here/tree.p" "$here/bubble.p" \
	"$here/bubble-order.p" "$here/merge.p" "$here/generic-interfaces.p" > "$directory/source.p"
source "$here/check-functions.sh"
check 0 generic-interfaces "$directory/source.p"
sed '/ :: /{ :next; /;$/d; N; b next; }' "$directory/source.p" > "$directory/independent.p"
check 0 independent-synthesis "$directory/independent.p"
printf '%s\n' 'all five source backends accept arbitrary payload A; Bubble keeps explicit transitivity'
