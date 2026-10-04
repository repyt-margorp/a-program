#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
root=$(dirname "${BASH_SOURCE[0]}")
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
provider="$directory/provider.p"
sed '/^import /d' "$root/fixtures/sorted-proof-provider.p" "$root/fixtures/finite_positions.p" \
	"$root/fixtures/finite_vectors.p" "$root/fixtures/finite_list_views.p" \
	"$root/fixtures/generic_sorted/content-proof.p" > "$directory/base.p"
sed '/^import /d' "$directory/base.p" "$root/fixtures/finite_permutation_views.p" > "$provider"
source="$root/acceptance/finite-permutation-views.p"

check() {
	local expected=$1 label=$2 status=0
	shift 2
	# A local query cycle must fail the test instead of ignoring the outer fuel.
	timeout 120 "$binary" --legacy-intrinsic-dot --steps 8000000 "$@" > "$directory/status" || status=$?
	printf '%s: ' "$label"
	cat "$directory/status"
	[[ $status == "$expected" ]]
	case $expected in
		0) grep -q '^done steps=' "$directory/status" ;;
		1) grep -q '^rejected steps=' "$directory/status" ;;
		3) grep -q '^pending steps=' "$directory/status" ;;
	esac
}
equal() {
	timeout 120 "$compare" --steps 8000000 --equal-image "$1" "$2" "$3"
}
check 0 general-permutation-bridge --imports "$directory/base.p" "$root/fixtures/finite_permutation_views.p"
check 0 finite-action --imports "$provider" --save "$directory/action.a" "$source"
for pair in main:output first_origin:one second_origin:two third_origin:zero \
	first_value:one second_value:one third_value:two singleton_origin:zero; do
	equal "$directory/action.a" "${pair%:*}" "${pair#*:}"
done
sed '/^import /d; / :: /{ :next; /;$/d; N; b next; }' "$provider" "$source" > "$directory/independent.p"
check 0 independent-synthesis --save "$directory/independent.a" "$directory/independent.p"
equal "$directory/independent.a" main output
# Typed results are an explicit larger profile; the product allowance stays fixed.
check 0 independent-materialized --save-materialized "$directory/materialized.a" "$directory/independent.p"
status=0
timeout 120 "$binary" --load --steps 0 "$directory/materialized.a" > "$directory/status" 2> "$directory/error" || status=$?
[[ $status == 2 ]]
grep -q 'cannot read or initialize input' "$directory/error"
timeout 120 "$compare" --image-limit 2000000 --steps 8000000 --equal-image "$directory/materialized.a" main output
sed '/^import /d' "$provider" "$source" > "$directory/complete.p"
negative() {
	local label=$1 body=$2
	printf '%s\n' 'import Nat; import List; import Fin; import same_position; import permutation;' \
		'import position_identity; import position_permutation; import position_forward;' \
		'import reordering; import reordering_positions; import reordering_observe;' \
		'import list_size; import list_sized; import permutation_reordering; import sized_lookup;' \
		'import zero; import one; import two; import three; import empty; import singleton;' \
		'import input; import output; import composed; import input_size; import certificate;' \
		'import same_nat; import first; import positions;' "$body" > "$directory/$label.p"
	check 1 "$label" --imports "$directory/complete.p" "$directory/$label.p"
}
negative wrong-length 'bad:=permutation_reordering Nat input output composed two input_size;'
negative omitted-element 'bad:=permutation_reordering Nat input singleton composed three input_size;'
negative duplicated-element 'bad:=permutation_reordering Nat input ((List Nat).cons one output) composed three input_size;'
negative wrong-value 'bad:=reordering_observe Nat three input input_size output certificate first &(\v:Nat=>same_nat two v) (same_nat.refl two);'
negative wrong-origin 'bad:=(same_position three).refl first :: same_position three first (position_forward three positions first);'
negative duplicated-position 'bad:=(position_permutation three).mk &(\i:Fin three=>first) &(\i:Fin three=>i) &(\i:Fin three=>(same_position three).refl i) &(\i:Fin three=>(same_position three).refl i);'

check 0 "semantic-save" --imports "$provider" --save "$directory/full.a" "$source"
check 0 "semantic-load" --load "$directory/full.a"
equal "$directory/full.a" second_origin two
for budget in 0 100; do
	check 3 "semantic-pending-$budget" --imports "$provider" --steps "$budget" \
		--save "$directory/partial.a" "$source"
	check 3 "semantic-inert-$budget" --load --steps 0 \
		--save "$directory/copy.a" "$directory/partial.a"
	cmp "$directory/partial.a" "$directory/copy.a"
	check 0 "semantic-resume-$budget" --load "$directory/copy.a"
	equal "$directory/copy.a" main output
done
check 3 "semantic-invalid-pending" --imports "$directory/complete.p" --steps 100 \
	--save "$directory/wrong.a" "$directory/wrong-value.p"
check 1 "semantic-invalid-resume" --load "$directory/wrong.a"
printf '%s\n' 'finite permutation views: general position bijection and actual-value laws passed'
