#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
mode=${3:-lists}
case $mode in lists|views) ;; *) exit 2;; esac
steps=${SORTING_CHECK_STEPS:-40000000}
started=$SECONDS
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
bash "$here/provider.sh" "$here/merge.p" "$here/../../../tests/fixtures/generic_sorted/boolean-order.p" \
	"$here/merge-cases.p" > "$directory/source.p"
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
		3) grep -q '^pending steps=' "$directory/status" ;;
	esac
}
equal() {
	timeout 180 "$compare" --steps "$steps" --equal-image "$directory/source.a" "$1" "$2"
}
check 0 merge-source --save "$directory/source.a" "$directory/source.p"
sed '/ :: /{ :next; /;$/d; N; b next; }' "$directory/source.p" > "$directory/independent.p"
check 0 independent-synthesis "$directory/independent.p"
equal merge_report merge_report_expected
equal merge_unfinished merge_reversed
status=0
equal merge_report merge_report_wrong || status=$?
[[ $status == 1 ]]
for bad in \
	'bad:=merge_fuel_local &merge_order &merge_compare &merge_decide Nat.zero merge_reversed (merge_fits.nil Nat.zero);' \
	'bad:=merge_backend &merge_order &(\x:Nat=>\y:Nat=>Bool.true) &merge_decide;' \
	'bad:=merge_certificate :: permutation Nat merge_duplicates merge_empty;'; do
	{ sed -n '1,$p' "$directory/source.p"; printf '%s\n' "$bad"; } > "$directory/wrong.p"
	check 1 rejected "$directory/wrong.p"
done
check 0 ordinary-load --load "$directory/source.a"
check 3 pending --steps 100 --save "$directory/pending.a" "$directory/source.p"
check 3 inert --load --steps 0 --save "$directory/copy.a" "$directory/pending.a"
cmp "$directory/pending.a" "$directory/copy.a"
check 0 resume --load "$directory/copy.a"
if [[ $mode == views ]]; then
	equal merge_vector merge_expected
	equal merge_origin merge_origin_expected
fi
printf 'legacy merge (%s): passed in %s seconds\n' "$mode" "$((SECONDS-started))"
