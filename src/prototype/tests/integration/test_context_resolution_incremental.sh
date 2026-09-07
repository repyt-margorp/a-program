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

pending_equation=$(awk '
	/A_PROGRAM_CONTEXT_RESOLUTION_TRACE 1 context=/ &&
		/classifier=4294967295/ {
		for (i = 1; i <= NF; ++i) {
			if ($i ~ /^equation=/) {
				sub(/^equation=/, "", $i)
				print $i
				exit
			}
		}
	}
' "$tmp_dir/trace")
resolved_equation=$(awk -v equation="$pending_equation" '
	/A_PROGRAM_CONTEXT_RESOLUTION_TRACE 1 context=/ {
		classifier = ""
		candidate = ""
		for (i = 1; i <= NF; ++i) {
			if ($i ~ /^classifier=/) {
				classifier = $i
				sub(/^classifier=/, "", classifier)
			} else if ($i ~ /^equation=/) {
				candidate = $i
				sub(/^equation=/, "", candidate)
			}
		}
		if (candidate == equation && classifier != "4294967295") {
			print classifier
			exit
		}
	}
' "$tmp_dir/trace")
if [ -z "$pending_equation" ] || [ -z "$resolved_equation" ]; then
	echo 'Context classifier equation was not resolved through its single answer cell' >&2
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
*' binder_owner_rebuilds=0') ;;
*)
	echo 'Binding owner index was reconstructed instead of maintained' >&2
	exit 1
	;;
esac

echo 'incremental Context resolution checks passed'
