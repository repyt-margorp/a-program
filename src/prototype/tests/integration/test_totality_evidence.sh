#!/bin/sh
set -eu

# Boundary audit: ISSUE-14-TERMINATION-EVIDENCE-BOUNDARY

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

source_file=src/prototype/tests/fixtures/typing/totality_evidence_surface_check.p
./read_file.out "$source_file" >"$tmp_dir/source.out"
grep -q '^term termination := APP(TERMINATES_WITNESS_FORMER' "$tmp_dir/source.out"
grep -q '\[terminates-total-computation proof#' "$tmp_dir/source.out"
grep -q '^term effectfulTermination := APP(TERMINATES_WITNESS_FORMER' \
	"$tmp_dir/source.out"

./read_file.out --write-artifact "$tmp_dir/totality.apo" "$source_file" \
	>"$tmp_dir/write.out"
./read_file.out --read-graph "$tmp_dir/totality.apo" >"$tmp_dir/read.out"
grep -q '^interface term termination ' "$tmp_dir/read.out"
grep -Eq '^derivation [0-9]+ 54 claim [0-9]+ premises 2$' \
	"$tmp_dir/totality.apo"

if ./read_file.out src/prototype/tests/fixtures/negative/returns_removed.p \
	>"$tmp_dir/removed.out" 2>"$tmp_dir/removed.err"; then
	echo "removed #.returns syntax was accepted" >&2
	exit 1
fi

echo "totality evidence checks passed"
