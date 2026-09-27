#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
root=$(dirname "${BASH_SOURCE[0]}")
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
source="$root/acceptance/family-domain-conversion.p"

check() {
	local expected=$1 label=$2 status=0
	shift 2
	"$binary" --legacy-intrinsic-dot --steps 1000000 "$@" > "$directory/status" || status=$?
	printf '%s: ' "$label"
	cat "$directory/status"
	[[ $status == "$expected" ]]
	case $expected in
		0) grep -q '^done steps=' "$directory/status" ;;
		1) grep -q '^rejected steps=' "$directory/status" ;;
		4) grep -q '^unsupported steps=' "$directory/status" ;;
		3) grep -q '^pending steps=' "$directory/status" ;;
	esac
}

check 0 family-conversion --save "$directory/full.a" "$source"
for pair in main:expected back:back_expected variable:expected selected_main:selected_expected; do
	"$compare" --equal-image "$directory/full.a" "${pair%:*}" "${pair#*:}"
done
sed '/ :: /d' "$source" > "$directory/no-assertions.p"
check 0 independent-synthesis "$directory/no-assertions.p"

negative() {
	local label=$1 body=$2 expected=${3:-1}
	printf '%s\n' 'import Nat;' 'import Box;' 'import Family;' 'import Reverse;' \
		'import higher;' 'import Holder;' "$body" > "$directory/$label.p"
	check "$expected" "$label" --imports "$source" "$directory/$label.p"
}
negative wrong-bound 'bad:=\k:Nat=>\b:Box k=>Family (Nat.succ k) b;'
negative wrong-nominal 'Other:=@\n:Nat=>{mk:(k:Nat)->* k;};
	bad:=Family Nat.zero (Other.mk Nat.zero);'
negative wrong-family 'bad:=higher Box Nat.zero (Box.mk Nat.zero);'
negative wrong-result 'bad:=\k:Nat=>\b:Box k=>b;
	bad :: (k:Nat)->Box k->Box (Nat.succ k);'
negative effectful-index 'bad:=Family Nat.zero {#print #"not a type-level computation"; Box.mk Nat.zero;};' 4

for mode in ordinary retained; do
	options=()
	if [[ $mode == retained ]]; then options+=(--retain-reductions); fi
	check 0 "$mode-index-declaration" "${options[@]}" --save "$directory/index.a" \
		"$root/acceptance/family-index-domain.p"
	check 0 "$mode-index-load" --load "$directory/index.a"
	check 0 "$mode-save" "${options[@]}" --save "$directory/complete.a" "$source"
	check 0 "$mode-load" --load "$directory/complete.a"
	for budget in 0 100; do
		check 3 "$mode-partial-$budget" --steps "$budget" "${options[@]}" \
			--save "$directory/partial.a" "$source"
		check 3 "$mode-inert-$budget" --load --steps 0 "${options[@]}" \
			--save "$directory/copy.a" "$directory/partial.a"
		cmp "$directory/partial.a" "$directory/copy.a"
		check 0 "$mode-resume-$budget" --load "$directory/copy.a"
		"$compare" --equal-image "$directory/copy.a" main expected
	done
	check 3 "$mode-invalid-partial" --imports "$source" --steps 0 "${options[@]}" \
		--save "$directory/wrong.a" "$directory/wrong-bound.p"
	check 1 "$mode-invalid-resume" --load "$directory/wrong.a"
done
# The repaired conversion is also needed by a general indexed lookup proof.
sed '/^import /d' "$root/fixtures/sorted-proof-provider.p" "$root/fixtures/finite_positions.p" \
	"$root/fixtures/finite_vectors.p" > "$directory/fin.p"
vector="$root/acceptance/finite-vector-lookup.p"
check 0 vector-source --imports "$directory/fin.p" --save "$directory/vector.a" "$vector"
for pair in main:one lookup_first:one lookup_second:zero; do
	"$compare" --equal-image "$directory/vector.a" "${pair%:*}" "${pair#*:}"
done
sed '/ :: /{ :next; /;$/d; N; b next; }' "$vector" > "$directory/vector-independent.p"
sed '/ :: /{ :next; /;$/d; N; b next; }' "$directory/fin.p" > "$directory/independent-provider.p"
check 0 vector-independent --imports "$directory/independent-provider.p" "$directory/vector-independent.p"
for mode in ordinary retained; do
	options=()
	if [[ $mode == retained ]]; then options+=(--retain-reductions); fi
	check 0 "$mode-vector-save" --imports "$directory/fin.p" "${options[@]}" --save "$directory/vector.a" "$vector"
	check 0 "$mode-vector-load" --load "$directory/vector.a"
	check 3 "$mode-vector-partial" --imports "$directory/fin.p" --steps 100 "${options[@]}" \
		--save "$directory/vector-partial.a" "$vector"
	check 0 "$mode-vector-resume" --load "$directory/vector-partial.a"
done
sed '/^import /d' "$directory/fin.p" "$vector" > "$directory/vector-provider.p"
printf '%s\n' 'import Nat; import Fin; import Vec; import vec_lookup; import sample; import two;' \
	'bad:=vec_lookup Nat two sample (Fin.zero two);' > "$directory/vector-bound.p"
check 1 vector-wrong-bound --imports "$directory/vector-provider.p" "$directory/vector-bound.p"
printf '%s\n' 'import Nat; import Fin; import vec_at; import vec_lookup_at;' \
	'import sample; import one; import two;' \
	'bad:=vec_lookup_at Nat two (Fin.zero one) sample :: vec_at Nat two sample (Fin.succ (Fin.zero Nat.zero)) one;' \
	> "$directory/vector-position.p"
check 1 vector-wrong-position --imports "$directory/vector-provider.p" "$directory/vector-position.p"
printf '%s\n' 'family application: checked conversion, independent assertions and images passed'
