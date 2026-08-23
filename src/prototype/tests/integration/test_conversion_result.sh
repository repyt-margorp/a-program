#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-conversion-result.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

if rg -n \
	'prototype_term_normalization_equal|prototype_judgement_classifier_normalization_equal|classifier_kernel_normalization_equal' \
	src/prototype --glob '*.[ch]'; then
	echo "legacy boolean conversion API remains" >&2
	exit 1
fi

prototype_compile c11 werror graph \
	"$tmp_dir/conversion-result-check" \
	src/prototype/tests/checks/conversion_result_check.c

"$tmp_dir/conversion-result-check"
