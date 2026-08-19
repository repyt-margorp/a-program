#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
fixture=src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p
runner="$tmp_dir/process-metrics"
limit_ms=${A_PROGRAM_IF8_SINGLE_COMPILE_LIMIT_MS:-10000}

make -f src/prototype/Makefile reader >/dev/null
cc -std=c11 -Wall -Wextra -Werror \
	src/prototype/tests/support/process_metrics.c -o "$runner"

fixture_hash=$(sha256sum "$fixture" | awk '{ print $1 }')
compiler=$(cc --version | sed -n '1p' | tr ' ' '_')
machine=$(uname -srm | tr ' ' '_')
commit=$(git rev-parse --verify HEAD 2>/dev/null || printf unknown)
dirty=0
if ! git diff --quiet --ignore-submodules -- || \
	! git diff --cached --quiet --ignore-submodules --
then
	dirty=1
fi
printf 'A_PROGRAM_BENCHMARK_ENV 1 fixture=%s fixture_sha256=%s commit=%s dirty=%s compiler=%s machine=%s flags=-std=c11_-Wall_-Wextra\n' \
	"$fixture" "$fixture_hash" "$commit" "$dirty" "$compiler" "$machine"

./read_file.out "$fixture" >"$tmp_dir/warmup.out"

: >"$tmp_dir/wall-ms"
run=1
while [ "$run" -le 3 ]; do
	metrics="$tmp_dir/run-$run.metrics"
	A_PROGRAM_PERFORMANCE_COUNTERS=1 "$runner" \
		./read_file.out "$fixture" >"$tmp_dir/run-$run.out" 2>"$metrics"
	line=$(grep '^A_PROGRAM_PROCESS_METRICS 1 ' "$metrics")
	wall_us=$(printf '%s\n' "$line" | sed -n 's/.* wall_us=\([0-9][0-9]*\).*/\1/p')
	wall_ms=$((wall_us / 1000))
	printf '%s\n' "$wall_ms" >>"$tmp_dir/wall-ms"
	printf 'A_PROGRAM_IF8_SINGLE_COMPILE 1 run=%s %s\n' "$run" \
		"$(printf '%s' "$line" | sed 's/^A_PROGRAM_PROCESS_METRICS 1 //')"
	grep '^A_PROGRAM_\(PERFORMANCE\|SOLVER\|CONTEXT_RESOLUTION\)_COUNTERS 1 ' \
		"$metrics"
	solver_line=$(grep '^A_PROGRAM_SOLVER_COUNTERS 1 ' "$metrics")
	case $solver_line in
	*' context_index_rebuilds=0 substitution_index_rebuilds=0') ;;
	*)
		echo 'IF8 source compilation performed a bulk Context/Substitution rebuild' >&2
		exit 1
		;;
	esac
	context_line=$(grep '^A_PROGRAM_CONTEXT_RESOLUTION_COUNTERS 1 ' "$metrics")
	case $context_line in
	*' skips=0 '*)
		echo 'IF8 Context resolver did not skip any clean request' >&2
		exit 1
		;;
	*' binder_owner_rebuilds=0') ;;
	*)
		echo 'IF8 reconstructed the Binding owner index' >&2
		exit 1
		;;
	esac
	run=$((run + 1))
done

median_ms=$(sort -n "$tmp_dir/wall-ms" | sed -n '2p')
printf 'A_PROGRAM_IF8_SINGLE_COMPILE_SUMMARY 1 median_wall_ms=%s limit_ms=%s\n' \
	"$median_ms" "$limit_ms"
if [ "$median_ms" -gt "$limit_ms" ]; then
	echo "IF8 single compile exceeded limit: ${median_ms}ms > ${limit_ms}ms" >&2
	exit 1
fi
