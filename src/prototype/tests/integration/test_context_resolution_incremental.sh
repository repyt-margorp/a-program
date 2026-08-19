#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
make -f src/prototype/Makefile reader >/dev/null

fixture=src/prototype/tests/fixtures/typing/indexed_branch_rebuild_check.p
env A_PROGRAM_CONTEXT_RESOLUTION_TRACE=1 A_PROGRAM_PERFORMANCE_COUNTERS=1 \
	./read_file.out "$fixture" >"$tmp_dir/output" 2>"$tmp_dir/trace"

global_count=$(grep -c 'source_contexts=.* all_dirty=1 ' "$tmp_dir/trace")
targeted_count=$(grep -c 'source_contexts=.* all_dirty=0 ' "$tmp_dir/trace")
binding_count=$(grep -c 'cause=binding$' "$tmp_dir/trace")
targeted_local_count=$(awk '
	/source_contexts=.* all_dirty=0 / { targeted = 1; next }
	/source_contexts=/ { targeted = 0 }
	targeted && / context=/ { count++ }
	END { print count + 0 }
' "$tmp_dir/trace")
targeted_source_count=$(awk '
	/source_contexts=.* all_dirty=0 / {
		for (i = 1; i <= NF; ++i) {
			if ($i ~ /^source_contexts=/) {
				sub(/^source_contexts=/, "", $i)
				print $i
				exit
			}
		}
	}
' "$tmp_dir/trace")
if [ "$global_count" -ne 2 ] || [ "$targeted_count" -eq 0 ] || \
	[ "$binding_count" -eq 0 ] || [ -z "$targeted_source_count" ] || \
	[ "$targeted_local_count" -ge "$targeted_source_count" ]; then
	echo 'Context resolution did not preserve initial/targeted/final scheduling' >&2
	exit 1
fi

solver=$(grep '^A_PROGRAM_SOLVER_COUNTERS 1 ' "$tmp_dir/trace")
context=$(grep '^A_PROGRAM_CONTEXT_RESOLUTION_COUNTERS 1 ' "$tmp_dir/trace")
case $solver in
*' context_index_rebuilds=0 substitution_index_rebuilds=0') ;;
*)
	echo 'ordinary lowering performed a bulk Context/Substitution rebuild' >&2
	exit 1
	;;
esac
case $context in
*' skips=0 '*)
	echo 'clean Context resolver requests were not skipped' >&2
	exit 1
	;;
*' binder_owner_rebuilds=0') ;;
*)
	echo 'Binding owner index was reconstructed instead of maintained' >&2
	exit 1
	;;
esac

echo 'incremental Context resolution checks passed'
