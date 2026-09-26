#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-checked-artifact.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
. src/prototype/build/test_support.sh
prototype_test_timing_initialize "$tmp_dir"

prototype_test_phase compile
prototype_compile c11 werror compiler \
	"$tmp_dir/checked-artifact-container-check" \
	src/prototype/tests/checks/checked_artifact_container_check.c

prototype_test_phase execute
"$tmp_dir/checked-artifact-container-check"

prototype_test_phase_finish
