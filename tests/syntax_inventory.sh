#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
tests="$root/tests"
runner=${1:?usage: syntax_inventory.sh /path/to/parse_files}
report=$(mktemp)
trap 'rm -f "$report"' EXIT
cd "$root"
mapfile -t programs < <(awk -F '\t' '$1 == "program" {print $3}' "$tests/compatibility.tsv")
test "${#programs[@]}" -gt 0
status=0
"$runner" --legacy-intrinsic-dot "${programs[@]}" > "$report" || status=$?
if (( status > 1 )); then
	cat "$report"
	exit "$status"
fi
awk -F '\t' -v expected="${#programs[@]}" '
NR == FNR { excluded[$1] = $2 FS $3; next }
{
	if (seen[$2]++) { print "duplicate result: " $2; failed = 1 }
	if ($2 in excluded) {
		if ($1 != "syntax_error" || $3 FS $4 != excluded[$2]) {
			print "changed exclusion: " $0; failed = 1
		}
		checked_exclusions[$2] = 1
	} else if ($1 != "parsed") {
		print "unexpected syntax failure: " $0; failed = 1
	}
	count++
}
END {
	for (path in excluded) if (!(path in checked_exclusions)) {
		print "missing exclusion result: " path; failed = 1
	}
	if (count != expected) { print "incomplete inventory: " count "/" expected; failed = 1 }
	if (!failed) print "syntax inventory: " count " results match reviewed syntax expectations (not semantic acceptance)"
	exit failed
}' "$tests/syntax_exclusions.tsv" "$report"
