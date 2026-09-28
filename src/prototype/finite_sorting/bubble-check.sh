#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
steps=${SORTING_CHECK_STEPS:-40000000}
image_options=(--image-limit "${SORTING_IMAGE_LIMIT:-3000000}")
started=$SECONDS
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
fixtures="$here/../../../tests/fixtures"
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
source "$here/check-functions.sh"
bash "$here/provider.sh" "$here/bubble.p" "$here/bubble-order.p" "$here/quick.p" \
	"$fixtures/generic_sorted/boolean-order.p" "$here/cases.p" "$here/bubble-cases.p" > "$directory/source.p"
check 0 bubble-source --save "$directory/source.a" "$directory/source.p"
sed '/ :: /{ :next; /;$/d; N; b next; }' "$directory/source.p" > "$directory/independent.p"
check 0 independent-synthesis "$directory/independent.p"
equal "$directory/source.a" bubble_report bubble_report_expected
equal "$directory/source.a" bubble_once bubble_once_expected
status=0
equal "$directory/source.a" bubble_report bubble_report_wrong || status=$?
[[ $status == 1 ]]
negative() {
	local name=$1 declaration=$2
	{ sed -n '1,$p' "$directory/source.p"; printf '%s\n' "$declaration"; } > "$directory/wrong.p"
	check 1 "$name" "$directory/wrong.p"
}
negative wrong-comparator 'bad:=bubble_transitive_backend Item &item_order &(\x:Item=>\y:Item=>Bool.true) &item_decide &item_trans;'
negative wrong-transitivity 'bad:=bubble_transitive_backend Item &item_order &item_le &item_decide &(\x:Item=>\y:Item=>\p:item_order x y=>\z:Item=>\q:item_order y z=>bool_order.top);'
negative wrong-content 'bad:=bubble_content_certificate duplicates :: permutation Item duplicates empty;'
negative wrong-value 'bad:=sorting_vector_value Item &item_order bubble_items three vector_input first &(\v:Item=>same_label two (item_label v)) (same_label.refl two);'
negative wrong-size 'bad:=bubble_sort_sized Item &item_le two bubble_input;'
for storage in ordinary retained; do
	options=()
	if [[ $storage == retained ]]; then
		options+=(--retain-reductions)
		check 0 retained-save "${options[@]}" --save "$directory/full.a" "$directory/source.p"
		equal "$directory/full.a" bubble_report bubble_report_expected
	fi
	check 3 "$storage-pending" --steps 100 "${options[@]}" --save "$directory/pending.a" "$directory/source.p"
	check 3 "$storage-inert" --load --steps 0 "${options[@]}" --save "$directory/copy.a" "$directory/pending.a"
	cmp "$directory/pending.a" "$directory/copy.a"
	equal "$directory/copy.a" bubble_report bubble_report_expected
done
bash "$here/provider.sh" "$here/bubble.p" "$fixtures/sortedness-cycle.p" "$here/bubble-cycle.p" > "$directory/cycle.p"
check 0 nontransitive-example --save "$directory/cycle.a" "$directory/cycle.p"
equal "$directory/cycle.a" bubble_cycle_result input
equal "$directory/cycle.a" bubble_cycle_certified input
printf '%s\n' 'bad:=bubble_cycle_local :: general_strongly_sorted Point Cycle bubble_cycle_result;' >> "$directory/cycle.p"
check 1 local-is-not-strong "$directory/cycle.p"
bash "$here/provider.sh" "$here/bubble.p" "$here/bubble-directional-counterexample.p" > "$directory/counterexample.p"
check 0 directional-counterexample --save "$directory/counterexample.a" "$directory/counterexample.p"
sed '/ :: /{ :next; /;$/d; N; b next; }' "$directory/counterexample.p" > "$directory/independent.p"
check 0 independent-counterexample "$directory/independent.p"
equal "$directory/counterexample.a" bubble_counter_result bubble_counter_input
printf '%s\n' 'bad:=bubble_edge_evidence bubble_point.a bubble_point.c bubble_relation.ca;' >> "$directory/counterexample.p"
check 1 wrong-edge-direction "$directory/counterexample.p"
printf 'bubble backend (transitive general order): passed in %s seconds\n' "$((SECONDS-started))"
