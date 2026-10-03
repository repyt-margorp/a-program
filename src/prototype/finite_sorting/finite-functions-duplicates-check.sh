#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
steps=${SORTING_CHECK_STEPS:-40000000}
image_options=(--image-limit "${SORTING_IMAGE_LIMIT:-10000000}")
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
bash "$here/provider.sh" "$here/finite-functions.p" "$here/quick.p" "$here/tree.p" \
	"$here/bubble.p" "$here/bubble-order.p" "$here/merge.p" \
	"$here/../../../tests/fixtures/generic_sorted/boolean-order.p" "$here/cases.p" \
	"$here/finite-functions-cases.p" "$here/finite-functions-duplicates.p" > "$directory/source.p"
source "$here/check-functions.sh"
check 0 duplicate-function-source --save "$directory/source.a" "$directory/source.p"
sed '/ :: /{ :next; /;$/d; N; b next; }' "$directory/source.p" > "$directory/independent.p"
check 0 duplicate-independent-synthesis "$directory/independent.p"
equal "$directory/source.a" finite_repeat_report finite_repeat_report_expected
{ cat "$directory/source.p"; printf '%s\n' 'bad:=(same_position three).refl (position_forward three (finite_repeat_positions finite_merge) first) :: same_position three (position_forward three (finite_repeat_positions finite_merge) first) (position_forward three (finite_repeat_positions finite_merge) second);'; } > "$directory/collapsed-positions.p"
check 1 equal-payload-positions-distinct --save "$directory/rejected.a" "$directory/collapsed-positions.p"
check 1 duplicate-rejected-load --load "$directory/rejected.a"
{ cat "$directory/source.p"; printf '%s\n' 'bad:=finite_repeat_content :: permutation Item finite_repeat_input ((List Item).cons low_one singleton);'; } > "$directory/lost-duplicate.p"
check 1 lost-identical-duplicate "$directory/lost-duplicate.p"
check 0 duplicate-ordinary-load --load "$directory/source.a"
check 3 duplicate-pending --steps 100 --save "$directory/pending.a" "$directory/source.p"
check 3 duplicate-inert --load --steps 0 --save "$directory/copy.a" "$directory/pending.a"
cmp "$directory/pending.a" "$directory/copy.a"
check 0 duplicate-resume --load --save "$directory/resumed.a" "$directory/copy.a"
equal "$directory/resumed.a" finite_repeat_report finite_repeat_report_expected
printf '%s\n' 'identical payloads: all five actual outputs, recovery, distinct origins/destinations, inverse consumers, two rejections and fresh-process images passed'
