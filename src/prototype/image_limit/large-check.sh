#!/usr/bin/env bash
set -euo pipefail
binary=$1
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
sorting="$here/../finite_sorting"
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
steps=10000000
source "$sorting/check-functions.sh"
bash "$sorting/provider.sh" "$sorting/quick.p" "$here/../../../tests/fixtures/generic_sorted/boolean-order.p" \
	"$sorting/cases.p" "$sorting/stress-wrong-function.p" > "$directory/wrong.p"
check 1 retained-rejected-save --retain-reductions --save "$directory/wrong.a" "$directory/wrong.p"
check 2 default-bound --load "$directory/wrong.a"
check 1 explicit-bound --load --image-limit 3000000 "$directory/wrong.a"
check 3 inert --load --image-limit 3000000 --steps 0 --retain-reductions --save "$directory/copy.a" "$directory/wrong.a"
cmp "$directory/wrong.a" "$directory/copy.a"
check 1 resumed-rejection --load --image-limit 3000000 "$directory/copy.a"
printf '%s\n' 'large retained image: explicit reader bound does not accept an invalid result certificate'
