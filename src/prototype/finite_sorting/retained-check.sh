#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
mode=${3:-insertion}
case $mode in insertion|quick|all) ;; *) exit 2;; esac
limit=${SORTING_IMAGE_LIMIT:-3000000}
steps=${SORTING_CHECK_STEPS:-40000000}
started=$SECONDS
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
bash "$here/provider.sh" "$here/quick.p" "$here/../../../tests/fixtures/generic_sorted/boolean-order.p" \
	"$here/cases.p" > "$directory/source.p"
timeout 180 "$binary" --legacy-intrinsic-dot --steps "$steps" --retain-reductions \
	--save "$directory/full.a" "$directory/source.p"
observe() {
	local algorithm
	for algorithm in quick insertion; do
		[[ $mode == all || $mode == "$algorithm" ]] || continue
		timeout 180 "$compare" --image-limit "$limit" --steps "$steps" --equal-image \
			"$1" "${algorithm}_report" "${algorithm}_report_expected"
	done
}
observe "$directory/full.a"
status=0
timeout 180 "$binary" --legacy-intrinsic-dot --steps 100 --retain-reductions \
	--save "$directory/pending.a" "$directory/source.p" || status=$?
[[ $status == 3 ]]
status=0
timeout 180 "$binary" --load --steps 0 --retain-reductions \
	--save "$directory/copy.a" "$directory/pending.a" || status=$?
[[ $status == 3 ]]
cmp "$directory/pending.a" "$directory/copy.a"
observe "$directory/copy.a"
printf 'retained API comparison (%s, limit %s): passed in %s seconds\n' "$mode" "$limit" "$((SECONDS-started))"
