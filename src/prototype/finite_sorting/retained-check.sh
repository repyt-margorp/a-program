#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
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
timeout 180 "$compare" --image-limit "$limit" --steps "$steps" --equal-image \
	"$directory/full.a" insertion_report insertion_report_expected
status=0
timeout 180 "$binary" --legacy-intrinsic-dot --steps 100 --retain-reductions \
	--save "$directory/pending.a" "$directory/source.p" || status=$?
[[ $status == 3 ]]
status=0
timeout 180 "$binary" --load --steps 0 --retain-reductions \
	--save "$directory/copy.a" "$directory/pending.a" || status=$?
[[ $status == 3 ]]
cmp "$directory/pending.a" "$directory/copy.a"
timeout 180 "$compare" --image-limit "$limit" --steps "$steps" --equal-image \
	"$directory/copy.a" insertion_report insertion_report_expected
printf 'retained API comparison (limit %s): passed in %s seconds\n' "$limit" "$((SECONDS-started))"
