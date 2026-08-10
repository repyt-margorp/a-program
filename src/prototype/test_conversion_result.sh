#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

if rg -n \
	'prototype_term_normalization_equal|prototype_judgement_classifier_normalization_equal|classifier_kernel_normalization_equal' \
	src/prototype --glob '*.[ch]'; then
	echo "legacy boolean conversion API remains" >&2
	exit 1
fi

prototype_compile c11 werror graph \
	/tmp/a-program-conversion-result-check \
	src/prototype/conversion_result_check.c

/tmp/a-program-conversion-result-check
rm -f /tmp/a-program-conversion-result-check
