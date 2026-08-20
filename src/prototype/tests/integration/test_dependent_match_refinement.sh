#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-dependent-match.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
. src/prototype/build/test_support.sh
prototype_test_timing_initialize "$tmp_dir"
make -f src/prototype/Makefile reader all >/dev/null

compare=src/prototype/tests/fixtures/typing/dependent_recursive_comparison_check.p
rebuild=src/prototype/tests/fixtures/typing/indexed_branch_rebuild_check.p
impossible=src/prototype/tests/fixtures/typing/impossible_index_branch_check.p
constant=src/prototype/tests/fixtures/typing/residual_index_equation_negative.p
residual=src/prototype/tests/fixtures/typing/dependent_residual_index_equation_negative.p
owner_rebuild=examples/type-infer-and-check/level2/02_tree.p

prototype_test_phase source
for fixture in compare rebuild impossible constant; do
	eval source=\$$fixture
	./read_file.out "$source" >"$tmp_dir/$fixture.out"
done
./read_file.out "$owner_rebuild" >"$tmp_dir/owner-rebuild.out"

prototype_test_phase publication
for fixture in compare rebuild impossible constant; do
	eval source=\$$fixture
	./read_file.out --write-artifact "$tmp_dir/$fixture.apo" "$source" \
		>"$tmp_dir/$fixture-write.out"
	grep -q '^A_PROGRAM_ARTIFACT 82 ' "$tmp_dir/$fixture.apo"
done
./read_file.out --write-artifact "$tmp_dir/owner-rebuild.apo" \
	"$owner_rebuild" >"$tmp_dir/owner-rebuild-write.out"
grep -q '^A_PROGRAM_ARTIFACT 82 ' "$tmp_dir/owner-rebuild.apo"

prototype_test_phase readback
for fixture in compare rebuild impossible constant; do
	./read_file.out --read-graph "$tmp_dir/$fixture.apo" \
		>"$tmp_dir/$fixture-read.out"
done

# This source exercises speculative definition lowering followed by occurrence
# slot reuse and zero-clause fold continuations.  Binder-owner lookup must be
# rebuilt from the retained occurrence graph rather than preserve stale slot
# ownership from the failed attempt.
./read_file.out --read-graph "$tmp_dir/owner-rebuild.apo" \
	>"$tmp_dir/owner-rebuild-read.out"
grep -q '^metadata label max ' "$tmp_dir/owner-rebuild.out"
grep -q '^metadata label height ' "$tmp_dir/owner-rebuild.out"

grep -q '\[ih-elim proof#' "$tmp_dir/compare.out"
grep -q '\[solved-match-motive proof#' "$tmp_dir/compare.out"
grep -q '\[ih-elim proof#' "$tmp_dir/rebuild.out"
grep -q '\[solved-match-motive proof#' "$tmp_dir/rebuild.out"

for output in "$tmp_dir/compare.out" "$tmp_dir/rebuild.out"; do
	grep -q 'refinement-status=1' "$output"
	if grep -Eq 'refinement-status=(0|3)' "$output"; then
		echo "accepted source retained a pending or residual Match action" >&2
		exit 1
	fi
done
grep -q 'refinement-status=2' "$tmp_dir/impossible.out"
grep -q 'refinement-status=4' "$tmp_dir/constant.out"

awk '
	$1 == "occurrence_match_case" {
		seen++
		if ($4 == 1 && $5 == 4294967295) bad = 1
		else if ($4 == 2 && $5 != 4294967295) bad = 1
		else if ($4 != 1 && $4 != 2) bad = 1
	}
	END { exit seen == 0 || bad }
' "$tmp_dir/compare.apo"
awk '
	$1 == "occurrence_match_case" && $4 == 2 && $5 == 4294967295 {
		found = 1
	}
	END { exit !found }
' "$tmp_dir/impossible.apo"
awk '
	$1 == "occurrence_match_case" && $4 == 4 && $5 == 4294967295 {
		found = 1
	}
	END { exit !found }
' "$tmp_dir/constant.apo"

prototype_test_phase runtime
printf ':q\n' | ./a.out "$compare" >"$tmp_dir/compare-runtime.out"
grep -Eq '^value main := RETURN\(APP\(CONSTRUCTOR\(rep#[0-9]+\.ordinal#1\), APP\(CONSTRUCTOR\(rep#[0-9]+\.ordinal#0\), APP\(CONSTRUCTOR\(rep#[0-9]+\.ordinal#1\), CONSTRUCTOR\(rep#[0-9]+\.ordinal#0\)\)\)\)\)$' \
	"$tmp_dir/compare-runtime.out"
printf ':q\n' | ./a.out "$rebuild" >"$tmp_dir/rebuild-runtime.out"
grep -Eq '^value main := RETURN\(APP\(CONSTRUCTOR\(rep#[0-9]+\.ordinal#0\), CONSTRUCTOR\(rep#[0-9]+\.ordinal#0\)\)\)$' \
	"$tmp_dir/rebuild-runtime.out"

prototype_test_phase negative
sed 's/currentToHead :: LE lower head/currentToHead :: LE head lower/' \
	"$rebuild" >"$tmp_dir/wrong-orientation.p"
if ./read_file.out "$tmp_dir/wrong-orientation.p" \
	>"$tmp_dir/wrong-orientation.out" 2>"$tmp_dir/wrong-orientation.err"; then
	echo "wrongly oriented branch proof unexpectedly compiled" >&2
	exit 1
fi
grep -q 'diagnostic-code=ascription-mismatch' "$tmp_dir/wrong-orientation.err"
grep -Eq 'span=[1-9][0-9]*:[1-9][0-9]*' "$tmp_dir/wrong-orientation.err"

if ./read_file.out "$residual" \
	>"$tmp_dir/residual.out" 2>"$tmp_dir/residual.err"; then
	echo "unsupported non-pattern index equation unexpectedly compiled" >&2
	exit 1
fi
grep -q 'diagnostic-code=branch-refinement-residual' "$tmp_dir/residual.err"
grep -Eq 'span=[1-9][0-9]*:[1-9][0-9]*' "$tmp_dir/residual.err"
if grep -q 'expected=term#4294967295 actual=term#4294967295' \
	"$tmp_dir/residual.err"; then
	echo "residual diagnostic discarded its unsolved equation" >&2
	exit 1
fi

awk '
	$1 == "occurrence_match_case" && $4 == 1 && !changed {
		$4 = 0
		changed = 1
	}
	{ print }
	END { if (!changed) exit 1 }
' "$tmp_dir/rebuild.apo" >"$tmp_dir/missing-status.apo"
if ./read_file.out --read-graph "$tmp_dir/missing-status.apo" \
	>"$tmp_dir/missing-status.out" 2>"$tmp_dir/missing-status.err"; then
	echo "artifact replay re-solved a missing branch action" >&2
	exit 1
fi

awk '
	$1 == "occurrence_match_case" && $4 == 1 && !changed {
		$5 = 0
		changed = 1
	}
	{ print }
	END { if (!changed) exit 1 }
' "$tmp_dir/rebuild.apo" >"$tmp_dir/wrong-action.apo"
if ./read_file.out --read-graph "$tmp_dir/wrong-action.apo" \
	>"$tmp_dir/wrong-action.out" 2>"$tmp_dir/wrong-action.err"; then
	echo "artifact replay accepted a branch action with wrong endpoints" >&2
	exit 1
fi

awk '
	$1 == "occurrence_match_case_binders" && $3 > 0 && !changed {
		$4 = 4294967294
		changed = 1
	}
	{ print }
	END { if (!changed) exit 1 }
' "$tmp_dir/rebuild.apo" >"$tmp_dir/wrong-binder.apo"
if ./read_file.out --read-graph "$tmp_dir/wrong-binder.apo" \
	>"$tmp_dir/wrong-binder.out" 2>"$tmp_dir/wrong-binder.err"; then
	echo "artifact replay accepted a forged branch BindingId" >&2
	exit 1
fi

for regression in \
	explicit_index_family_tail_check \
	explicit_index_family_acc_concrete_check
do
	./read_file.out "src/prototype/tests/fixtures/typing/$regression.p" \
		>"$tmp_dir/$regression.out"
done

prototype_test_phase_finish
echo "dependent Match refinement tests passed"
