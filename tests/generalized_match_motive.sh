#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
root=$(dirname "${BASH_SOURCE[0]}")
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
source="$root/acceptance/generalized-match-motive.p"
check() {
	local expected=$1 label=$2 status=0
	shift 2
	"$binary" --steps 200000 "$@" > "$directory/status" || status=$?
	printf '%s: ' "$label"
	cat "$directory/status"
	[[ $status == "$expected" ]]
	case $expected in
		0) grep -q '^done steps=' "$directory/status" ;;
		1) grep -q '^rejected steps=' "$directory/status" ;;
		3) grep -q '^pending steps=' "$directory/status" ;;
	esac
}
check 0 generalized-motives --save "$directory/full.a" "$source"
for pair in main:expected roundtrip:sample empty_flip:empty singleton_flip:singleton replacement_result:replaced; do
	"$compare" --equal-image "$directory/full.a" "${pair%:*}" "${pair#*:}"
done
sed '/ :: /d' "$source" > "$directory/independent.p"
check 0 assertion-free "$directory/independent.p"
sed 's/@(n self => Vec A (Nat.succ n))/@(n self => Vec A n)/g' "$source" > "$directory/wrong-motive.p"
check 1 wrong-motive "$directory/wrong-motive.p"
sed 's/Vec A n->Vec A n;/Vec A n->Vec A (Nat.succ n);/' "$source" > "$directory/wrong-assertion.p"
check 1 wrong-assertion "$directory/wrong-assertion.p"
check 0 "semantic-save" --save "$directory/full.a" "$source"
check 0 "semantic-load" --load "$directory/full.a"
for budget in 0 100 1000; do
	check 3 "semantic-pending-$budget" --steps "$budget" --save "$directory/partial.a" "$source"
	check 0 "semantic-resume-$budget" --load "$directory/partial.a"
done
check 3 "semantic-invalid-pending" --steps 100 --save "$directory/wrong.a" "$directory/wrong-motive.p"
check 1 "semantic-invalid-resume" --load "$directory/wrong.a"
printf '%s\n' 'generalized explicit motives: captured values, shadowing, post-checks and images passed'
