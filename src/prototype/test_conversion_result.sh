#!/bin/sh
set -eu

if rg -n \
	'prototype_term_normalization_equal|prototype_judgement_classifier_normalization_equal|classifier_kernel_normalization_equal' \
	src/prototype --glob '*.[ch]'; then
	echo "legacy boolean conversion API remains" >&2
	exit 1
fi

cc -std=c11 -Wall -Wextra -Werror -I src/prototype \
	src/prototype/conversion_result_check.c \
	src/prototype/ast.c \
	src/prototype/reader.c \
	src/prototype/term.c \
	src/prototype/type_declaration.c \
	src/prototype/typing.c \
	src/prototype/universe.c \
	src/prototype/symbol.c \
	-o /tmp/a-program-conversion-result-check

/tmp/a-program-conversion-result-check
rm -f /tmp/a-program-conversion-result-check
