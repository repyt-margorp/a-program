#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
root=$(dirname "${BASH_SOURCE[0]}")
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
source="$root/acceptance/indexed-constructor-sequencing.p"
check() {
	local expected=$1 label=$2 status=0
	shift 2
	"$binary" --steps 300000 "$@" > "$directory/status" || status=$?
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
results() {
	local input=$1
	for name in main late_main alias_result partial_result late_partial_result; do
		"$compare" --equal-image "$input" "$name" expected
	done
	"$compare" --equal-image "$input" effect_result effect_expected
	"$compare" --equal-image "$input" unused_result unused_expected
}
check 0 dependent-constructor --save "$directory/full.a" "$source"
results "$directory/full.a"
sed '/ :: /d' "$source" > "$directory/unasserted.p"
check 0 assertion-free "$directory/unasserted.p"
sed 's/((same Nat).refl one));/((same Nat).refl zero));/g' "$source" > "$directory/wrong-proof.p"
check 1 wrong-proof "$directory/wrong-proof.p"
sed 's/packet A x;/packet A (boxed A x);/' "$source" > "$directory/wrong-assertion.p"
check 1 wrong-assertion "$directory/wrong-assertion.p"
sed '$a bad := (packet #Text).mk ((same #Text).refl #"hello") (boxed #Text (#print #"hello")) ((same #Text).refl #"hello");' \
	"$source" > "$directory/effect-result.p"
# A handler can change print's result. Dependent sequencing of this effectful
# value is unsupported; the compiler must not assume it equals the request.
check 4 effect-result-not-known "$directory/effect-result.p"
for fixture in inferred-index-later-recovery inferred-index-dependent inferred-index-declaration \
	inferred-index-two-inputs inferred-index-copy inferred-index-effects; do
	check 0 "$fixture" --save "$directory/existing.a" "$root/acceptance/$fixture.p"
	"$compare" --equal-image "$directory/existing.a" main expected
	if [[ $fixture == inferred-index-effects ]]; then
		"$compare" --equal-image "$directory/existing.a" spineMain spineExpected
	fi
done
for fixture in inferred-index-earlier-wrong inferred-index-disagreement inferred-index-wrong-family inferred-index-partial-dependent; do
	check 1 "$fixture" "$root/acceptance/$fixture.p"
done
for mode in ordinary retained; do
	options=()
	if [[ $mode == retained ]]; then options+=(--retain-reductions); fi
	check 0 "$mode-save" "${options[@]}" --save "$directory/full.a" "$source"
	check 0 "$mode-load" --load "$directory/full.a"
	results "$directory/full.a"
	for budget in 0 100 1000; do
		check 3 "$mode-pending-$budget" --steps "$budget" "${options[@]}" --save "$directory/partial.a" "$source"
		check 0 "$mode-resume-$budget" --load "$directory/partial.a"
	done
	check 3 "$mode-invalid-pending" --steps 100 "${options[@]}" --save "$directory/wrong.a" "$directory/wrong-proof.p"
	check 1 "$mode-invalid-resume" --load "$directory/wrong.a"
done
printf '%s\n' 'indexed constructor sequencing: dependent proofs, recovery, partial calls, effects and images passed'
