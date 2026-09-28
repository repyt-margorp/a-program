#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
mode=${3:-all}
steps=${SORTING_CHECK_STEPS:-40000000}
image_options=()
if [[ -n ${SORTING_IMAGE_LIMIT:-} ]]; then image_options+=(--image-limit "$SORTING_IMAGE_LIMIT"); fi
case $mode in all|source|lists|quick|insertion) ;; *) printf 'unknown check mode: %s\n' "$mode" >&2; exit 2;; esac
started=$SECONDS
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
provider="$directory/provider.p"
bash "$here/provider.sh" "$here/quick.p" "$here/../../../tests/fixtures/generic_sorted/boolean-order.p" > "$provider"
source "$here/check-functions.sh"
sed '/^import /d' "$provider" "$here/cases.p" > "$directory/cases.p"
check 0 general-and-closed --save "$directory/cases.a" "$directory/cases.p"
if [[ $mode == all || $mode == lists ]]; then
	equal "$directory/cases.a" quick_list_report quick_list_expected
	equal "$directory/cases.a" insertion_list_report insertion_list_expected
fi
if [[ $mode == all || $mode == quick ]]; then
	equal "$directory/cases.a" quick_report quick_report_expected
	status=0
	equal "$directory/cases.a" quick_report wrong_quick_report || status=$?
	[[ $status == 1 ]]
fi
if [[ $mode == all || $mode == insertion ]]; then
	equal "$directory/cases.a" insertion_report insertion_report_expected
fi
if [[ $mode == lists || $mode == quick || $mode == insertion ]]; then
	printf 'sorting backend observations (%s): passed in %s seconds\n' "$mode" "$((SECONDS-started))"
	exit 0
fi

# Assertions cannot supply the functions, indices or evidence used above.
sed '/ :: /{ :next; /;$/d; N; b next; }' "$directory/cases.p" > "$directory/independent.p"
check 0 independent-synthesis "$directory/independent.p"
negative() {
	local name=$1 body=$2
	# Generated test input; declarations are identical to the accepted source.
	{ sed -n '1,$p' "$directory/cases.p"; printf '%s\n' "$body"; } > "$directory/$name.p"
	check 1 "$name" "$directory/$name.p"
}
negative wrong-function-domain 'bad:=(sorting_backend Item &item_order).mk &(\x:Bool=>x) &(sorting_local Item &item_order quick_items) &(sorting_content Item &item_order quick_items);'
sed -n '1,$p' "$directory/cases.p" "$here/stress-wrong-function.p" > "$directory/wrong-function.p"
check 1 same-domain-function "$directory/wrong-function.p"
negative dropped-content 'bad:=(sorting_backend Item &item_order).mk &(\xs:List Item=>empty) &(\xs:List Item=>(general_locally_sorted Item &item_order).nil) &(\xs:List Item=>permutation_refl Item xs);'
negative wrong-comparator 'bad:=insertion_backend Item &item_order &(\x:Item=>\y:Item=>Bool.true) &item_decide;'
negative wrong-shape 'bad:=sorting_vector Item &item_order quick_items two vector_input;'
negative wrong-action 'bad:=position_action_compose Item three (position_flip three) (position_identity three) &(sized_lookup Item three duplicates input_size) first &(\v:Item=>same_label zero (item_label v)) (same_label.refl zero);'
for storage in ordinary retained; do
	options=()
	if [[ $storage == retained ]]; then options+=(--retain-reductions); fi
	check 0 "$storage-save" "${options[@]}" --save "$directory/full.a" "$directory/cases.p"
	check 0 "$storage-load" --load "${image_options[@]}" "$directory/full.a"
	if [[ $mode == all ]]; then equal "$directory/full.a" main insertion_expected; fi
	check 3 "$storage-pending" --steps 100 "${options[@]}" --save "$directory/pending.a" "$directory/cases.p"
	check 3 "$storage-inert" --load "${image_options[@]}" --steps 0 "${options[@]}" --save "$directory/copy.a" "$directory/pending.a"
	cmp "$directory/pending.a" "$directory/copy.a"
	check 0 "$storage-resume" --load "${image_options[@]}" "$directory/copy.a"
	if [[ $mode == all ]]; then equal "$directory/copy.a" quick_vector quick_expected; fi
	check 3 "$storage-invalid-pending" --steps 100 "${options[@]}" --save "$directory/wrong.a" "$directory/wrong-shape.p"
	check 1 "$storage-invalid-resume" --load "${image_options[@]}" "$directory/wrong.a"
done
printf 'sorting backend (%s): passed in %s seconds\n' "$mode" "$((SECONDS-started))"
