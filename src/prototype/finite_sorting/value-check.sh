#!/usr/bin/env bash
set -euo pipefail
binary=$1
steps=${SORTING_CHECK_STEPS:-40000000}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
bash "$here/provider.sh" "$here/quick.p" "$here/../../../tests/fixtures/generic_sorted/boolean-order.p" \
	"$here/cases.p" > "$directory/base.p"
check() {
	local expected=$1 label=$2 status=0
	shift 2
	timeout 180 "$binary" --legacy-intrinsic-dot --steps "$steps" "$@" > "$directory/status" || status=$?
	printf '%s: ' "$label"
	cat "$directory/status"
	[[ $status == "$expected" ]]
	case $expected in
		0) grep -q '^done steps=' "$directory/status" ;;
		1) grep -q '^rejected steps=' "$directory/status" ;;
	esac
}
sed -n '1,$p' "$directory/base.p" "$here/stress-value.p" > "$directory/positive.p"
check 0 concrete-values "$directory/positive.p"
sed '/ :: /{ :next; /;$/d; N; b next; }' "$directory/positive.p" > "$directory/independent.p"
check 0 independent-synthesis "$directory/independent.p"
negative() {
	local name=$1 declaration=$2
	{ sed -n '1,$p' "$directory/base.p"; printf '%s\n' "$declaration"; } > "$directory/$name.p"
	check 1 "$name" "$directory/$name.p"
}
negative wrong-quick-value 'bad := sorting_vector_value Item &item_order quick_items three vector_input first &(\v:Item => same_label one (item_label v)) (same_label.refl one);'
negative wrong-insertion-value 'bad := sorting_value Item &item_order insertion_items three duplicates input_size first &(\v:Item => same_label two (item_label v)) (same_label.refl two);'
printf '%s\n' 'concrete value transport: positive, assertion-free and incorrect labels passed'
