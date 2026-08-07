#!/bin/sh
set -eu

app_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_APP,' src/prototype/term.h)
lambda_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_LAMBDA,' src/prototype/term.h)
pi_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_PI,' src/prototype/term.h)
match_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_MATCH,' src/prototype/term.h)

test "$app_tags" -eq 1
test "$lambda_tags" -eq 1
test "$pi_tags" -eq 1
test "$match_tags" -eq 1

if grep -Eq 'PROTOTYPE_TERM_(VALUE|COMPUTATION)_(APP|LAMBDA|PI|MATCH)' \
		src/prototype/term.h; then
	echo "duplicated value/computation graph tag found" >&2
	exit 1
fi

if grep -Eq '^[[:space:]]*PROTOTYPE_TERM_(OBS_EQ|EQUALITY|PATH|TRANSPORT|COHERENCE)' \
		src/prototype/term.h; then
	echo "premature object-equality graph tag found" >&2
	exit 1
fi

cc -std=c11 -Wall -Wextra -I src/prototype \
	src/prototype/shared_term_reindex_check.c \
	src/prototype/ast.c \
	src/prototype/context.c \
	src/prototype/reader.c \
	src/prototype/term.c \
	src/prototype/type_declaration.c \
	src/prototype/typing.c \
	src/prototype/universe.c \
	src/prototype/symbol.c \
	-o /tmp/a-program-shared-term-reindex-check

/tmp/a-program-shared-term-reindex-check
rm -f /tmp/a-program-shared-term-reindex-check
