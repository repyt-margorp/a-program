#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-term-child-roles.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 werror compiler \
	"$tmp_dir/term-child-role-check" \
	src/prototype/tests/checks/term_child_role_check.c

"$tmp_dir/term-child-role-check"
