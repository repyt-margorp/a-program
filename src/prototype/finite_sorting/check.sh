#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
mode=${3:-all}
steps=${SORTING_CHECK_STEPS:-40000000}
image_options=(--image-limit "${SORTING_IMAGE_LIMIT:-10000000}")
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
check 0 semantic-load --load "$directory/cases.a"
check 3 recompute-export --load --steps 0 --save-inputs "$directory/inputs.a" "$directory/cases.a"
[[ $(wc -c < "$directory/inputs.a") -lt $(wc -c < "$directory/cases.a") ]]
printf 'artifact profiles: retained_bytes=%s inputs_bytes=%s conversion_steps=0\n' \
	"$(wc -c < "$directory/cases.a")" "$(wc -c < "$directory/inputs.a")"
check 3 recompute-inert --load --steps 0 --save "$directory/inputs-copy.a" "$directory/inputs.a"
cmp "$directory/inputs.a" "$directory/inputs-copy.a"
check 0 recompute-solve --load "$directory/inputs-copy.a"
if [[ $mode == all ]]; then equal "$directory/inputs-copy.a" quick_report quick_report_expected; fi
if [[ $mode == all ]]; then equal "$directory/cases.a" main insertion_expected; fi
check 3 semantic-pending --steps 100 --save "$directory/pending.a" "$directory/cases.p"
check 3 semantic-inert --load --steps 0 --save "$directory/copy.a" "$directory/pending.a"
cmp "$directory/pending.a" "$directory/copy.a"
check 0 semantic-resume --load "$directory/copy.a"
if [[ $mode == all ]]; then equal "$directory/copy.a" quick_vector quick_expected; fi
check 3 semantic-invalid-pending --steps 100 --save "$directory/wrong.a" "$directory/wrong-shape.p"
check 1 semantic-invalid-resume --load "$directory/wrong.a"
# A large materialized prefix is descriptive even when the final assertion fails.
check 1 semantic-invalid-materialized --save "$directory/wrong-full.a" "$directory/wrong-shape.p"
check 3 semantic-invalid-inert --load --steps 0 --save "$directory/wrong-full-copy.a" "$directory/wrong-full.a"
cmp "$directory/wrong-full.a" "$directory/wrong-full-copy.a"
check 1 semantic-invalid-materialized-resume --load "$directory/wrong-full-copy.a"
check 3 recompute-invalid-export --load --steps 0 --save-inputs "$directory/wrong-inputs.a" "$directory/wrong-full.a"
check 1 recompute-invalid-solve --load "$directory/wrong-inputs.a"
printf 'sorting backend (%s): passed in %s seconds\n' "$mode" "$((SECONDS-started))"
