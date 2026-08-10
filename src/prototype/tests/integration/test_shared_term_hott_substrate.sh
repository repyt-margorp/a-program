#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

app_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_APP([[:space:]]*=|,)' src/prototype/term.h)
lambda_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_LAMBDA([[:space:]]*=|,)' src/prototype/term.h)
pi_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_PI([[:space:]]*=|,)' src/prototype/term.h)
match_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_MATCH([[:space:]]*=|,)' src/prototype/term.h)

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

prototype_compile c11 warnings graph \
	/tmp/a-program-shared-term-reindex-check \
	src/prototype/tests/checks/shared_term_reindex_check.c

/tmp/a-program-shared-term-reindex-check
rm -f /tmp/a-program-shared-term-reindex-check
