#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-producer-session.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
. src/prototype/build/test_support.sh
prototype_test_timing_initialize "$tmp_dir"

prototype_test_phase compile
prototype_compile c11 werror compiler \
	"$tmp_dir/compile-producer-session-check" \
	src/prototype/tests/checks/compile_producer_session_check.c

prototype_test_phase execute
"$tmp_dir/compile-producer-session-check"

prototype_test_phase_finish
echo 'compile producer session tests passed'
