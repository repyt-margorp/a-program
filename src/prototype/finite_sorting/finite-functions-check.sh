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
	"$here/finite-functions-cases.p" > "$directory/source.p"
source "$here/check-functions.sh"
check 0 finite-function-source --save "$directory/source.a" "$directory/source.p"
sed '/ :: /{ :next; /;$/d; N; b next; }' "$directory/source.p" > "$directory/independent.p"
check 0 independent-synthesis "$directory/independent.p"
equal "$directory/source.a" finite_report finite_report_expected
equal "$directory/source.a" finite_small_report finite_small_expected
negative() {
	local name=$1 body=$2
	{ cat "$directory/source.p"; printf '%s\n' "$body"; } > "$directory/$name.p"
	check 1 "$name" "$directory/$name.p"
}
negative wrong-function-domain 'bad:=sorting_function Item &item_order finite_merge three &(\i:Nat=>high);'
negative wrong-position-bound 'bad:=sorting_function Item &item_order finite_merge three &finite_values (Fin.zero one);'
negative wrong-output-label 'bad:=sorting_function_value_back Item &item_order finite_merge three &finite_values first &(\v:Item=>same_label one (item_label v)) (same_label.refl one);'
negative wrong-recovery-label 'bad:=sorting_function_recover Item &item_order finite_merge three &finite_values second &(\v:Item=>same_label two (item_label v)) (same_label.refl two);'
negative forward-as-backward 'bad:=sorting_function_recover Item &item_order insertion_items three &finite_values first &(\v:Item=>same_label zero (item_label v)) (same_label.refl zero) :: same_label zero (item_label (sorting_function Item &item_order insertion_items three &finite_values (position_forward three (finite_positions insertion_items) first)));'
negative local-as-strong 'bad:=finite_merge_local :: general_strongly_sorted Item &item_order (sorting_function_contents Item &item_order finite_merge three &finite_values);'
negative lost-duplicate 'bad:=finite_merge_content :: permutation Item duplicates ((List Item).cons low_one singleton);'
check 0 ordinary-load --load "$directory/source.a"
check 3 pending --steps 100 --save "$directory/pending.a" "$directory/source.p"
check 3 inert --load --steps 0 --save "$directory/copy.a" "$directory/pending.a"
cmp "$directory/pending.a" "$directory/copy.a"
check 0 resume --load --save "$directory/resumed.a" "$directory/copy.a"
equal "$directory/resumed.a" finite_report finite_report_expected
check 1 rejected-save --save "$directory/rejected.a" "$directory/forward-as-backward.p"
check 1 rejected-load --load "$directory/rejected.a"
check 3 rejected-pending --steps 100 --save "$directory/rejected-pending.a" "$directory/forward-as-backward.p"
check 3 rejected-inert --load --steps 0 --save "$directory/rejected-copy.a" "$directory/rejected-pending.a"
cmp "$directory/rejected-pending.a" "$directory/rejected-copy.a"
check 1 rejected-resume --load "$directory/rejected-copy.a"
printf '%s\n' 'finite-function API: all five actual results/origins/destinations, recovery, independent synthesis, seven rejections and ordinary/pending/inert/rejected images passed'
