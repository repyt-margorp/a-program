#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
steps=${SORTING_CHECK_STEPS:-40000000}
image_options=(--image-limit "${SORTING_IMAGE_LIMIT:-10000000}")
started=$SECONDS
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
bash "$here/provider.sh" "$here/merge.p" "$here/../../../tests/fixtures/generic_sorted/boolean-order.p" \
	"$here/merge-generic-cases.p" > "$directory/source.p"
source "$here/check-functions.sh"
check 0 generic-merge-source --save "$directory/source.a" "$directory/source.p"
sed '/ :: /{ :next; /;$/d; N; b next; }' "$directory/source.p" > "$directory/independent.p"
check 0 independent-synthesis "$directory/independent.p"
equal "$directory/source.a" merge_bool_report merge_bool_report_expected
equal "$directory/source.a" merge_item_output merge_item_expected
equal "$directory/source.a" merge_item_vector merge_item_expected
equal "$directory/source.a" merge_item_origins merge_item_origins_expected
negative() {
	local name=$1 body=$2
	{ sed -n '1,$p' "$directory/source.p"; printf '%s\n' "$body"; } > "$directory/$name.p"
	check 1 "$name" "$directory/$name.p"
}
negative wrong-element-domain 'bad:=merge_sort_by Bool &(\x:Nat=>\y:Nat=>Bool.true) merge_bool_duplicates;'
negative wrong-order-law 'bad:=merge_backend Bool &bool_order &(\x:Bool=>\y:Bool=>Bool.true) &bool_decide;'
negative lost-occurrence 'bad:=merge_sort_content Bool &bool_le merge_bool_duplicates :: permutation Bool merge_bool_duplicates ((List Bool).cons Bool.false merge_bool_single);'
negative insufficient-fuel 'bad:=merge_fuel_local Bool &bool_order &bool_le &bool_decide Nat.zero merge_bool_reversed ((merge_fits Bool).nil Nat.zero);'
negative wrong-vector-shape 'bad:=sorting_vector merge_item &merge_item_order merge_item_backend merge_item_two merge_item_vector_input;'
negative wrong-origin-value 'bad:=sorting_vector_value merge_item &merge_item_order merge_item_backend merge_item_three merge_item_vector_input merge_item_first &(\v:merge_item=>merge_item_same_label merge_item_one (merge_item_label v)) (merge_item_same_label.refl merge_item_one);'
check 0 ordinary-load --load "$directory/source.a"
check 3 pending --steps 100 --save "$directory/pending.a" "$directory/source.p"
check 3 inert --load --steps 0 --save "$directory/copy.a" "$directory/pending.a"
cmp "$directory/pending.a" "$directory/copy.a"
check 0 resume --load "$directory/copy.a"
printf 'generic merge: passed in %s seconds\n' "$((SECONDS-started))"
