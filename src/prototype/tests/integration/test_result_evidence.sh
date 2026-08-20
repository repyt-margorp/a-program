#!/bin/sh
set -eu

# Boundary audit: ISSUE-13-EXPLICIT-RESULT-EVIDENCE
# Boundary audit: ISSUE-14-TERMINATION-EVIDENCE-BOUNDARY

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 werror graph \
	/tmp/a-program-result-evidence-check \
	src/prototype/tests/checks/result_evidence_check.c

/tmp/a-program-result-evidence-check
rm -f /tmp/a-program-result-evidence-check

source_file=src/prototype/tests/fixtures/typing/result_evidence_surface_check.p
./read_file.out "$source_file" >"$tmp_dir/surface.out"
grep -q '^term proof := APP(APP(RETURNS_WITNESS_FORMER' "$tmp_dir/surface.out"
grep -q '\[returns-evaluation proof#' "$tmp_dir/surface.out"
grep -q '^term termination := APP(TERMINATES_WITNESS_FORMER' \
	"$tmp_dir/surface.out"
grep -q '\[terminates-from-returns proof#' "$tmp_dir/surface.out"

./read_file.out --write-artifact "$tmp_dir/result-evidence.apo" "$source_file" \
	>"$tmp_dir/write.out"
./read_file.out --read-graph "$tmp_dir/result-evidence.apo" \
	>"$tmp_dir/read.out"
grep -q '^interface term proof ' "$tmp_dir/read.out"
grep -q '^interface term termination ' "$tmp_dir/read.out"
grep -Eq '^derivation [0-9]+ 50 claim [0-9]+ premises 3$' \
	"$tmp_dir/result-evidence.apo"
grep -Eq '^derivation [0-9]+ 52 claim [0-9]+ premises 2$' \
	"$tmp_dir/result-evidence.apo"

if ./read_file.out \
	src/prototype/tests/fixtures/typing/result_evidence_termination_mismatch_negative.p \
	>"$tmp_dir/termination-negative.out" 2>"$tmp_dir/termination-negative.err"; then
	echo "termination evidence accepted Returns evidence for a different computation" >&2
	exit 1
fi

dependent_source=src/prototype/tests/fixtures/typing/result_evidence_dependent_check.p
./read_file.out "$dependent_source" >"$tmp_dir/dependent-source.out"
grep -q '^term chooseReturns := APP(APP(RETURNS_WITNESS_FORMER' \
	"$tmp_dir/dependent-source.out"
grep -q '^term certifiedChoose := ' "$tmp_dir/dependent-source.out"
grep -q '^term consumeOpenResult := ' "$tmp_dir/dependent-source.out"

./read_file.out --write-artifact "$tmp_dir/dependent-result-evidence.apo" \
	"$dependent_source" >"$tmp_dir/dependent-write.out"
./read_file.out --read-graph "$tmp_dir/dependent-result-evidence.apo" \
	>"$tmp_dir/dependent-read.out"
grep -q '^interface term chooseReturns ' "$tmp_dir/dependent-read.out"
grep -q '^interface term certifiedChoose ' "$tmp_dir/dependent-read.out"
grep -q '^interface term consumeOpenResult ' "$tmp_dir/dependent-read.out"
grep -Eq '^derivation [0-9]+ 50 claim [0-9]+ premises 3$' \
	"$tmp_dir/dependent-result-evidence.apo"

echo "result evidence checks passed"
