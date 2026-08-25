#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-checked-core-session.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
. src/prototype/build/test_support.sh
prototype_test_timing_initialize "$tmp_dir"

prototype_test_phase dependency-boundary
if rg -n \
	'#include[[:space:]]+"a_program/(frontend|graph|kernel/judgement|artifact/publication)/' \
	src/prototype/include/a_program/checker; then
	echo 'checker public headers depend on producer or accepted-proof APIs' >&2
	exit 1
fi
if rg -n \
	'accepted_replay|validate_accepted|candidate_publication|solver_solution' \
	src/prototype/src/checker; then
	echo 'checker implementation depends on producer authority' >&2
	exit 1
fi

prototype_test_phase compile
prototype_compile c11 werror compiler \
	"$tmp_dir/checked-core-session-check" \
	src/prototype/tests/checks/checked_core_session_check.c

prototype_test_phase execute
"$tmp_dir/checked-core-session-check"

prototype_test_phase_finish
echo 'checked Core session tests passed'
