#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
source_fixture=src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p
suffix_fixture=src/prototype/tests/fixtures/typing/function_graph_quicksort_benchmark_suffix.p
fixture="$tmp_dir/function-graph-quicksort.p"
runner="$tmp_dir/process-metrics"
limit_ms=${A_PROGRAM_FUNCTION_GRAPH_QUICKSORT_LIMIT_MS:-2500}
fixed_point_limit_ns=${A_PROGRAM_FUNCTION_GRAPH_FIXED_POINT_LIMIT_NS:-700000000}
accepted_replay_limit_ns=${A_PROGRAM_FUNCTION_GRAPH_ACCEPTED_REPLAY_LIMIT_NS:-750000000}

{
	sed -n '1,$p' "$source_fixture"
	sed -n '1,$p' "$suffix_fixture"
} >"$fixture"

make -f src/prototype/Makefile reader >/dev/null
cc -std=c11 -Wall -Wextra -Werror \
	src/prototype/tests/support/process_metrics.c -o "$runner"

source_hash=$(sha256sum "$source_fixture" | awk '{ print $1 }')
suffix_hash=$(sha256sum "$suffix_fixture" | awk '{ print $1 }')
compiler=$(cc --version | sed -n '1p' | tr ' ' '_')
machine=$(uname -srm | tr ' ' '_')
commit=$(git rev-parse --verify HEAD 2>/dev/null || printf unknown)
dirty=0
if ! git diff --quiet --ignore-submodules -- || \
	! git diff --cached --quiet --ignore-submodules --
then
	dirty=1
fi
printf 'A_PROGRAM_BENCHMARK_ENV 1 source=%s source_sha256=%s suffix=%s suffix_sha256=%s commit=%s dirty=%s compiler=%s machine=%s flags=-std=c11_-Wall_-Wextra\n' \
	"$source_fixture" "$source_hash" "$suffix_fixture" "$suffix_hash" \
	"$commit" "$dirty" "$compiler" "$machine"

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
	printf 'A_PROGRAM_FUNCTION_GRAPH_QUICKSORT_SINGLE_COMPILE 1 run=%s %s\n' \
		"$run" "$(printf '%s' "$line" | sed 's/^A_PROGRAM_PROCESS_METRICS 1 //')"
	grep '^A_PROGRAM_\(PERFORMANCE\|SOLVER\|SOLVER_KIND\|CONTEXT_RESOLUTION\|CONSTRUCTOR_SPECIALIZATION\|TYPE_INSTANCE_CACHE\|COMPILE_PHASE\|FUNCTION_GRAPH_PHASE\|PROOF_MATERIALIZATION\|ACCEPTED_REPLAY\)_COUNTERS 1 ' \
		"$metrics"
	phase_line=$(grep '^A_PROGRAM_FUNCTION_GRAPH_PHASE_COUNTERS 1 ' "$metrics")
	compile_phase_line=$(grep '^A_PROGRAM_COMPILE_PHASE_COUNTERS 1 ' "$metrics")
	generated_nodes=$(printf '%s\n' "$phase_line" |
		sed -n 's/.* generated_ast_nodes=\([0-9][0-9]*\).*/\1/p')
	generated_types=$(printf '%s\n' "$phase_line" |
		sed -n 's/.* generated_types=\([0-9][0-9]*\).*/\1/p')
	if [ -z "$generated_nodes" ] || [ "$generated_nodes" -eq 0 ] ||
		[ -z "$generated_types" ] || [ "$generated_types" -eq 0 ]
	then
		echo 'Function Graph benchmark generated no graph AST/type data' >&2
		exit 1
	fi
	fixed_point_ns=$(printf '%s\n' "$compile_phase_line" |
		sed -n 's/.* fixed_point_ns=\([0-9][0-9]*\).*/\1/p')
	accepted_replay_ns=$(printf '%s\n' "$compile_phase_line" |
		sed -n 's/.* accepted_replay_ns=\([0-9][0-9]*\).*/\1/p')
	if [ -z "$fixed_point_ns" ] || [ "$fixed_point_ns" -gt "$fixed_point_limit_ns" ]; then
		echo "Function Graph fixed point exceeded limit: ${fixed_point_ns:-missing}ns > ${fixed_point_limit_ns}ns" >&2
		exit 1
	fi
	if [ -z "$accepted_replay_ns" ] ||
		[ "$accepted_replay_ns" -gt "$accepted_replay_limit_ns" ]; then
		echo "Function Graph accepted replay exceeded limit: ${accepted_replay_ns:-missing}ns > ${accepted_replay_limit_ns}ns" >&2
		exit 1
	fi
	if ! cmp -s "$tmp_dir/warmup.out" "$tmp_dir/run-$run.out"; then
		echo 'Function Graph instrumentation changed deterministic compiler output' >&2
		exit 1
	fi
	run=$((run + 1))
done

median_ms=$(sort -n "$tmp_dir/wall-ms" | sed -n '2p')
printf 'A_PROGRAM_FUNCTION_GRAPH_QUICKSORT_SINGLE_COMPILE_SUMMARY 1 median_wall_ms=%s limit_ms=%s\n' \
	"$median_ms" "$limit_ms"
if [ "$median_ms" -gt "$limit_ms" ]; then
	echo "Function Graph QuickSort exceeded limit: ${median_ms}ms > ${limit_ms}ms" >&2
	exit 1
fi
