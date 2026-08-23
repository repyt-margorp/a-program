#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-term-identity-frame.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 werror compiler \
	"$tmp_dir/term-identity-frame-check" \
	src/prototype/tests/checks/term_identity_frame_check.c

"$tmp_dir/term-identity-frame-check"
