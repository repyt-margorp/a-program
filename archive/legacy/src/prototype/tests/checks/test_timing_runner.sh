#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
runner="$root_dir/src/prototype/tests/run_integration_suite.sh"
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-test-runner-check.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

make_test() {
	directory=$1
	name=$2
	body=$3
	mkdir -p "$directory"
	{
		printf '%s\n' '#!/bin/sh'
		printf '%s\n' "$body"
	} >"$directory/$name"
}

pass_dir="$tmp_dir/pass"
make_test "$pass_dir" test_pass.sh 'echo pass-output'
sh "$runner" --test-directory "$pass_dir" \
	--timing-output "$tmp_dir/pass.timing" \
	>"$tmp_dir/pass.out" 2>"$tmp_dir/pass.err"
grep -q '^pass-output$' "$tmp_dir/pass.out"
grep -Eq '^A_PROGRAM_TEST_TIMING 1 suite=integration test=test_pass status=pass exit=0 wall_ms=[0-9]+$' \
	"$tmp_dir/pass.timing"
grep -Eq '^A_PROGRAM_TEST_SUMMARY 1 suite=integration total=1 executed=1 passed=1 failed=0 skipped=0 wall_ms=[0-9]+$' \
	"$tmp_dir/pass.timing"

filter_dir="$tmp_dir/filter"
make_test "$filter_dir" test_first.sh 'echo first'
make_test "$filter_dir" test_second.sh 'echo second'
sh "$runner" --test-directory "$filter_dir" --test-name test_second \
	--timing-output "$tmp_dir/filter.timing" \
	>"$tmp_dir/filter.out" 2>"$tmp_dir/filter.err"
grep -q '^second$' "$tmp_dir/filter.out"
if grep -q '^first$' "$tmp_dir/filter.out"; then
	echo 'single-test selection ran an unselected test' >&2
	exit 1
fi
grep -q 'total=1 executed=1 passed=1 failed=0 skipped=0 ' \
	"$tmp_dir/filter.timing"
if sh "$runner" --test-directory "$filter_dir" --test-name test_missing \
		>"$tmp_dir/missing.out" 2>"$tmp_dir/missing.err"; then
	echo 'runner accepted an unknown single-test name' >&2
	exit 1
else
	status=$?
fi
if [ "$status" -ne 2 ]; then
	echo 'unknown single-test name was not classified as runner misuse' >&2
	exit 1
fi

phase_dir="$tmp_dir/phase"
make_test "$phase_dir" test_phase.sh \
	'. "$A_PROGRAM_TEST_SUPPORT"
prototype_test_timing_initialize "${TMPDIR:-/tmp}"
prototype_test_phase first
prototype_test_phase second'
sh "$runner" --test-directory "$phase_dir" \
	--timing-output "$tmp_dir/phase.timing" \
	>"$tmp_dir/phase.out" 2>"$tmp_dir/phase.err"
if [ "$(grep -c '^A_PROGRAM_TEST_PHASE 1 ' "$tmp_dir/phase.timing")" -ne 2 ]; then
	echo 'runner did not close exactly two test phases' >&2
	exit 1
fi
grep -Eq 'test=test_phase phase=first status=pass exit=0 wall_ms=[0-9]+$' \
	"$tmp_dir/phase.timing"
grep -Eq 'test=test_phase phase=second status=pass exit=0 wall_ms=[0-9]+$' \
	"$tmp_dir/phase.timing"

failed_phase_dir="$tmp_dir/failed-phase"
make_test "$failed_phase_dir" test_failed_phase.sh \
	'. "$A_PROGRAM_TEST_SUPPORT"
prototype_test_timing_initialize "${TMPDIR:-/tmp}"
prototype_test_phase active
exit 6'
if sh "$runner" --test-directory "$failed_phase_dir" \
		--timing-output "$tmp_dir/failed-phase.timing" \
		>"$tmp_dir/failed-phase.out" 2>"$tmp_dir/failed-phase.err"; then
	echo 'runner accepted a failing open phase' >&2
	exit 1
else
	status=$?
fi
if [ "$status" -ne 6 ]; then
	echo 'runner lost the failing phase status' >&2
	exit 1
fi
grep -Eq 'test=test_failed_phase phase=active status=fail exit=6 wall_ms=[0-9]+$' \
	"$tmp_dir/failed-phase.timing"

fail_fast_dir="$tmp_dir/fail-fast"
make_test "$fail_fast_dir" test_01_fail.sh 'exit 7'
make_test "$fail_fast_dir" test_02_after.sh 'echo ran >"$A_PROGRAM_AFTER_MARKER"'
if A_PROGRAM_AFTER_MARKER="$tmp_dir/fail-fast-after" sh "$runner" \
		--test-directory "$fail_fast_dir" \
		--timing-output "$tmp_dir/fail-fast.timing" \
		>"$tmp_dir/fail-fast.out" 2>"$tmp_dir/fail-fast.err"; then
	echo 'fail-fast runner accepted a failing test' >&2
	exit 1
else
	status=$?
fi
if [ "$status" -ne 7 ] || [ -e "$tmp_dir/fail-fast-after" ]; then
	echo 'fail-fast runner lost the child status or ran a later test' >&2
	exit 1
fi
grep -q 'test=test_01_fail status=fail exit=7 ' "$tmp_dir/fail-fast.timing"
grep -q 'total=2 executed=1 passed=0 failed=1 skipped=1 ' "$tmp_dir/fail-fast.timing"

keep_going_dir="$tmp_dir/keep-going"
make_test "$keep_going_dir" test_01_fail.sh 'exit 9'
make_test "$keep_going_dir" test_02_after.sh 'echo ran >"$A_PROGRAM_AFTER_MARKER"'
if A_PROGRAM_AFTER_MARKER="$tmp_dir/keep-going-after" sh "$runner" \
		--keep-going --test-directory "$keep_going_dir" \
		--timing-output "$tmp_dir/keep-going.timing" \
		>"$tmp_dir/keep-going.out" 2>"$tmp_dir/keep-going.err"; then
	echo 'keep-going runner accepted a failing suite' >&2
	exit 1
else
	status=$?
fi
if [ "$status" -ne 1 ] || [ ! -e "$tmp_dir/keep-going-after" ]; then
	echo 'keep-going runner did not finish the suite' >&2
	exit 1
fi
grep -q 'test=test_01_fail status=fail exit=9 ' "$tmp_dir/keep-going.timing"
grep -q 'test=test_02_after status=pass exit=0 ' "$tmp_dir/keep-going.timing"
grep -q 'total=2 executed=2 passed=1 failed=1 skipped=0 ' "$tmp_dir/keep-going.timing"

signal_dir="$tmp_dir/signal"
make_test "$signal_dir" test_signal.sh 'kill -TERM $$'
if sh "$runner" --test-directory "$signal_dir" \
		--timing-output "$tmp_dir/signal.timing" \
		>"$tmp_dir/signal.out" 2>"$tmp_dir/signal.err"; then
	echo 'runner accepted a signaled test' >&2
	exit 1
else
	status=$?
fi
if [ "$status" -ne 143 ]; then
	echo 'runner did not preserve the signal-derived exit status' >&2
	exit 1
fi
grep -q 'test=test_signal status=signal exit=143 ' "$tmp_dir/signal.timing"

nested_dir="$tmp_dir/nested"
make_test "$nested_dir" test_nested.sh \
	'sh src/prototype/tests/integration/test_target.sh \'
if sh "$runner" --test-directory "$nested_dir" \
		>"$tmp_dir/nested.out" 2>"$tmp_dir/nested.err"; then
	echo 'runner accepted a nested integration test invocation' >&2
	exit 1
else
	status=$?
fi
if [ "$status" -ne 2 ]; then
	echo 'nested invocation was not classified as runner misuse' >&2
	exit 1
fi
grep -q '^nested integration test invocation:' "$tmp_dir/nested.err"

support_name_dir="$tmp_dir/support-name"
make_test "$support_name_dir" test_support_name.sh \
	'. src/prototype/build/test_support.sh'
sh "$runner" --test-directory "$support_name_dir" \
	>"$tmp_dir/support-name.out" 2>"$tmp_dir/support-name.err"

echo 'test timing runner checks passed'
