#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
root=$(dirname "${BASH_SOURCE[0]}")
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
provider="$directory/provider.p"
sed '/^import /d' "$root/../examples/02_nat.p" "$root/fixtures/finite_positions.p" > "$provider"
source="$root/acceptance/finite-positions.p"

check() {
	local expected=$1 label=$2 status=0
	shift 2
	"$binary" --steps 1000000 "$@" > "$directory/status" || status=$?
	printf '%s: ' "$label"
	cat "$directory/status"
	[[ $status == "$expected" ]]
	case $expected in
		0) grep -q '^done steps=' "$directory/status" ;;
		1) grep -q '^rejected steps=' "$directory/status" ;;
		3) grep -q '^pending steps=' "$directory/status" ;;
	esac
}

check 0 fin-import --imports "$root/../examples/02_nat.p" "$root/fixtures/finite_positions.p"
check 0 fin-source --imports "$provider" --save "$directory/positions.a" "$source"
for pair in main:one back:one twice:zero weakened:one duplicate_payload:zero \
	label_number:one singleton:zero unchanged:two identity:two \
	keep_first:zero keep_second:two keep_third:one keep_empty_value:zero \
	cycle_first:one cycle_second:two cycle_third:zero; do
	"$compare" --equal-image "$directory/positions.a" "${pair%:*}" "${pair#*:}"
done

# Deleting post-checks must not remove information needed to synthesize terms.
sed '/^import /d; / :: /{ :next; /;$/d; N; b next; }' "$provider" "$source" > "$directory/no-assertions.p"
check 0 independent-synthesis --save "$directory/independent.a" "$directory/no-assertions.p"
"$compare" --equal-image "$directory/independent.a" main one

negative() {
	local label=$1 body=$2
	printf '%s\n' 'import Nat;' 'import Fin;' 'import same_position;' \
		'import position_permutation;' "$body" > "$directory/$label.p"
	check 1 "$label" --imports "$provider" "$directory/$label.p"
}
negative empty-bound 'bad := Fin.zero Nat.zero :: Fin Nat.zero;'
negative missing-bound 'bad := Fin.zero :: Fin (Nat.succ Nat.zero);'
negative wrong-endpoint 'one:=Nat.succ Nat.zero; two:=Nat.succ one;
	left:=Fin.zero one; right:=Fin.succ (Fin.zero Nat.zero);
	bad := (same_position two).refl left :: same_position two left right;'
negative not-a-bijection 'one:=Nat.succ Nat.zero; two:=Nat.succ one;
	bad := (position_permutation two).mk &(\i:Fin two=>Fin.zero one) &(\i:Fin two=>i)
		&(\i:Fin two=>(same_position two).refl i) &(\i:Fin two=>(same_position two).refl i);'
negative wrong-inverse 'one:=Nat.succ Nat.zero; two:=Nat.succ one;
	bad := (position_permutation two).mk &(\i:Fin two=>i) &(\i:Fin two=>Fin.zero one)
		&(\i:Fin two=>(same_position two).refl i) &(\i:Fin two=>(same_position two).refl i);'
negative no-motive 'bad:=\n:Nat=>\left:Fin n=>\right:Fin n=>\p:same_position n left right=>p
	@refl i=>(same_position n).refl i;
	bad :: (n:Nat)->(left:Fin n)->(right:Fin n)->same_position n left right->same_position n right left;'
# The term still synthesizes, but the assertion must not change its motive.
sed '/bad ::/d' "$directory/no-motive.p" > "$directory/inferred-motive.p"
check 0 inferred-motive --imports "$provider" "$directory/inferred-motive.p"

for mode in ordinary retained; do
	options=()
	if [[ $mode == retained ]]; then options+=(--retain-reductions); fi
	check 0 "$mode-complete" --imports "$provider" "${options[@]}" --save "$directory/full.a" "$source"
	check 0 "$mode-load" --load "$directory/full.a"
	for budget in 0 100; do
		check 3 "$mode-pending-$budget" --imports "$provider" --steps "$budget" \
			"${options[@]}" --save "$directory/partial.a" "$source"
		check 3 "$mode-inert-$budget" --load --steps 0 "${options[@]}" \
			--save "$directory/copy.a" "$directory/partial.a"
		cmp "$directory/partial.a" "$directory/copy.a"
		check 0 "$mode-resume-$budget" --load "$directory/copy.a"
		"$compare" --equal-image "$directory/copy.a" main one
	done
	check 3 "$mode-negative-pending" --imports "$provider" --steps 100 \
		"${options[@]}" --save "$directory/wrong.a" "$directory/not-a-bijection.p"
	check 1 "$mode-negative-resume" --load "$directory/wrong.a"
done
printf '%s\n' 'finite positions: general inverse/composition proofs, labelled duplicates, assertions and images passed'
