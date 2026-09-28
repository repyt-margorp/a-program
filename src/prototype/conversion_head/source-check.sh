#!/usr/bin/env bash
set -euo pipefail
binary=$1
steps=${SORTING_CHECK_STEPS:-10000000}
started=$SECONDS
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
sorting="$here/../finite_sorting"
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
source "$sorting/check-functions.sh"
bash "$sorting/provider.sh" "$sorting/quick.p" "$here/../../../tests/fixtures/generic_sorted/boolean-order.p" \
	"$sorting/cases.p" "$sorting/stress-wrong-function.p" > "$directory/wrong.p"
check 1 same-domain-function --save "$directory/rejected.a" "$directory/wrong.p"
check 1 rejected-image --load "$directory/rejected.a"
for storage in ordinary retained; do
	options=()
	if [[ $storage == retained ]]; then options+=(--retain-reductions); fi
	check 3 "$storage-pending" --steps 100 "${options[@]}" --save "$directory/pending.a" "$directory/wrong.p"
	check 3 "$storage-inert" --load --steps 0 "${options[@]}" --save "$directory/copy.a" "$directory/pending.a"
	cmp "$directory/pending.a" "$directory/copy.a"
	check 1 "$storage-resume" --load "$directory/copy.a"
done
printf 'same-domain function rejection: passed in %s seconds\n' "$((SECONDS-started))"
