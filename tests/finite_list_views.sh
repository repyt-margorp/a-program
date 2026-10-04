#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
root=$(dirname "${BASH_SOURCE[0]}")
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
provider="$directory/provider.p"
sed '/^import /d' "$root/fixtures/sorted-proof-provider.p" "$root/fixtures/finite_positions.p" \
	"$root/fixtures/finite_vectors.p" > "$directory/vectors.p"
sed '/^import /d' "$directory/vectors.p" "$root/fixtures/finite_list_views.p" > "$provider"
source="$root/acceptance/finite-list-views.p"

check() {
	local expected=$1 label=$2 status=0
	shift 2
	"$binary" --legacy-intrinsic-dot --steps 2000000 "$@" > "$directory/status" || status=$?
	printf '%s: ' "$label"
	cat "$directory/status"
	[[ $status == "$expected" ]]
	case $expected in
		0) grep -q '^done steps=' "$directory/status" ;;
		1) grep -q '^rejected steps=' "$directory/status" ;;
		3) grep -q '^pending steps=' "$directory/status" ;;
	esac
}
check 0 imported-view-laws --imports "$directory/vectors.p" "$root/fixtures/finite_list_views.p"
check 0 view-source --imports "$provider" --save "$directory/view.a" "$source"
for pair in main:sample contents:sample rebuilt_contents:sample first:one second:zero shape:two \
	empty_roundtrip:empty singleton_roundtrip:singleton duplicates_roundtrip:duplicates; do
	"$compare" --equal-image "$directory/view.a" "${pair%:*}" "${pair#*:}"
done
sed '/^import /d; / :: /{ :next; /;$/d; N; b next; }' "$provider" "$source" > "$directory/independent.p"
check 0 independent-laws "$directory/independent.p"
sed '/^import /d' "$provider" "$source" > "$directory/complete.p"
negative() {
	local label=$1 body=$2
	printf '%s\n' 'import Nat; import List; import Vec; import list_size; import list_sized;' \
		'import vector_refill; import list_vector_covers; import list_vector_view;' \
		'import one; import two; import empty; import singleton; import sample; import duplicates; import vector;' \
		"$body" > "$directory/$label.p"
	check 1 "$label" --imports "$directory/complete.p" "$directory/$label.p"
}
negative wrong-length 'bad:=vector_refill Nat one sample (list_sized Nat sample);'
negative omitted-element 'bad:=list_sized Nat singleton :: list_size Nat two singleton;'
negative duplicated-element 'bad:=list_vector_covers Nat sample :: list_vector_view Nat two vector duplicates;'
negative reordered-elements 'bad:=list_vector_covers Nat sample :: list_vector_view Nat two vector ((List Nat).cons Nat.zero ((List Nat).cons one empty));'
negative wrong-shape 'bad:=vector_refill Nat two sample (list_sized Nat sample) :: Vec Nat one;'
check 0 "semantic-save" --imports "$provider" --save "$directory/full.a" "$source"
check 0 "semantic-load" --load "$directory/full.a"
for budget in 0 100; do
	check 3 "semantic-pending-$budget" --imports "$provider" --steps "$budget" \
		--save "$directory/partial.a" "$source"
	check 0 "semantic-resume-$budget" --load "$directory/partial.a"
done
check 3 "semantic-invalid-pending" --imports "$directory/complete.p" --steps 100 \
	--save "$directory/wrong.a" "$directory/wrong-length.p"
check 1 "semantic-invalid-resume" --load "$directory/wrong.a"
printf '%s\n' 'finite List/Vec views: general coverage, refill, roundtrip, assertions and images passed'
