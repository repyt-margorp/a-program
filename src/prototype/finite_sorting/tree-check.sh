#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
steps=${SORTING_CHECK_STEPS:-40000000}
limit=${SORTING_IMAGE_LIMIT:-3000000}
started=$SECONDS
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
fixtures="$here/../../../tests/fixtures"
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
bash "$here/provider.sh" "$here/tree.p" "$here/quick.p" "$fixtures/generic_sorted/boolean-order.p" \
	"$here/cases.p" "$here/tree-cases.p" > "$directory/source.p"
source "$here/check-functions.sh"
check 0 tree-source --save "$directory/source.a" "$directory/source.p"
sed '/ :: /{ :next; /;$/d; N; b next; }' "$directory/source.p" > "$directory/independent.p"
check 0 independent-synthesis "$directory/independent.p"
equal "$directory/source.a" tree_report tree_report_expected
status=0
equal "$directory/source.a" tree_report tree_report_wrong || status=$?
[[ $status == 1 ]]
for bad in \
	'bad:=tree_backend Item &item_order &(\x:Item=>\y:Item=>Bool.true) &item_decide;' \
	'bad:=tree_content_certificate duplicates :: permutation Item duplicates empty;' \
	'bad:=sorting_vector_value Item &item_order tree_items three vector_input first &(\v:Item=>same_label two (item_label v)) (same_label.refl two);'; do
	{ sed -n '1,$p' "$directory/source.p"; printf '%s\n' "$bad"; } > "$directory/wrong.p"
	check 1 rejected "$directory/wrong.p"
done
for storage in ordinary retained; do
	options=()
	if [[ $storage == retained ]]; then options+=(--retain-reductions); fi
	check 0 "$storage-save" "${options[@]}" --save "$directory/full.a" "$directory/source.p"
	equal "$directory/full.a" tree_report tree_report_expected
	check 3 "$storage-pending" --steps 100 "${options[@]}" --save "$directory/pending.a" "$directory/source.p"
	check 3 "$storage-inert" --load --steps 0 "${options[@]}" --save "$directory/copy.a" "$directory/pending.a"
	cmp "$directory/pending.a" "$directory/copy.a"
	equal "$directory/copy.a" tree_report tree_report_expected
done
bash "$here/provider.sh" "$here/tree.p" "$fixtures/sortedness-cycle.p" "$here/tree-cycle.p" > "$directory/cycle.p"
check 0 nontransitive --save "$directory/cycle.a" "$directory/cycle.p"
equal "$directory/cycle.a" cycle_tree_result cycle_tree_expected
equal "$directory/cycle.a" cycle_tree_certified cycle_tree_expected
printf '%s\n' 'bad:=cycle_tree_local :: general_strongly_sorted Point Cycle cycle_tree_result;' >> "$directory/cycle.p"
check 1 local-is-not-strong "$directory/cycle.p"
printf 'tree backend: passed in %s seconds\n' "$((SECONDS-started))"
