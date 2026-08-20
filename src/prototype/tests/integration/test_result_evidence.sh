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

sequence_source=src/prototype/tests/fixtures/typing/result_evidence_sequence_binding_check.p
./read_file.out "$sequence_source" >"$tmp_dir/sequence-source.out"
grep -q '\[returns-sequence-binding proof#' "$tmp_dir/sequence-source.out"
./read_file.out --write-artifact "$tmp_dir/sequence.apo" "$sequence_source" \
	>"$tmp_dir/sequence-write.out"
./read_file.out --read-graph "$tmp_dir/sequence.apo" >"$tmp_dir/sequence-read.out"
grep -Eq '^context [0-9]+ [0-9]+ [0-9]+ [0-9]+ 2 [0-9]+ [0-9]+$' \
	"$tmp_dir/sequence.apo"
grep -Eq '^derivation [0-9]+ 53 claim [0-9]+ premises 3$' \
	"$tmp_dir/sequence.apo"

indexed_source=src/prototype/tests/fixtures/typing/computation_indexed_family_check.p
./read_file.out "$indexed_source" >"$tmp_dir/computation-indexed.out"
grep -q '^type (Returns A) constructors=1$' "$tmp_dir/computation-indexed.out"

for negative in \
	result_evidence_sequence_wrong_computation_negative.p \
	result_evidence_ordinary_binder_negative.p
do
	if ./read_file.out "src/prototype/tests/fixtures/typing/$negative" \
		>"$tmp_dir/$negative.out" 2>"$tmp_dir/$negative.err"; then
		echo "invalid open Returns evidence was accepted: $negative" >&2
		exit 1
	fi
done

awk '
	$1 == "context" && $6 == 2 && !changed { $7 = 0; changed = 1 }
	{ print }
	END { if (!changed) exit 1 }
' "$tmp_dir/sequence.apo" >"$tmp_dir/sequence-corrupt.apo"
if ./read_file.out --read-graph "$tmp_dir/sequence-corrupt.apo" \
	>"$tmp_dir/sequence-corrupt.out" 2>"$tmp_dir/sequence-corrupt.err"; then
	echo "artifact replay accepted a forged computation-result Context origin" >&2
	exit 1
fi

echo "result evidence checks passed"
