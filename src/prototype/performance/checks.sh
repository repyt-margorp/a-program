#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
overlay=${1:?usage: checks.sh HEAD_ADAPTED_OVERLAY OUTPUT_DIRECTORY}
output=${2:?usage: checks.sh HEAD_ADAPTED_OVERLAY OUTPUT_DIRECTORY}
[[ $(cat "$overlay/variant.txt") == head-adapted ]]
mkdir "$output"
build="$overlay/build"
make -s -j2 -f "$here/build.mk" OVERLAY="$overlay" BUILD="$build" \
	CHECKPOINT_TESTS="$overlay/checkpoint_tests/" \
	"$build/core_test" "$build/iadt_test" "$build/eval_io_test" \
	"$build/program_test" "$build/performance_direct_test" "$build/performance_head_test" \
	"$build/artifact_normalization_checkpoint_test" "$build/artifact_source_checkpoint_test" \
	> "$output/build.log" 2>&1
for name in core_test iadt_test performance_direct_test performance_head_test \
	artifact_normalization_checkpoint_test artifact_source_checkpoint_test; do
	"$build/$name" > "$output/$name.log" 2>&1
done
bash "$overlay/tests/eval_io.sh" "$build/eval_io_test" > "$output/eval_io.log" 2>&1
for cut in {0..8}; do
	"$build/performance_head_test" write "$output/$cut.machine" "$cut"
	"$build/performance_head_test" read "$output/$cut.machine" >> "$output/head-reload.log"
done
"$build/program_test" --equal "$overlay/tests/acceptance/function-graph-captured-match.p" main expected > "$output/captured-main.log" 2>&1
"$build/program_test" --equal "$overlay/tests/acceptance/function-graph-captured-match.p" other two > "$output/captured-other.log" 2>&1
for name in function-graph-captured-match-wrong function-graph-named-wrong-proof; do
	"$build/program_test" --reject "$overlay/tests/acceptance/$name.p" > "$output/$name.log" 2>&1
done
printf 'Focused adapted epoch passed. Original Core failures are separate evidence.\n'
