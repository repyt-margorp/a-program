#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT_DIR"

manifest_fingerprint=$(sha256sum src/prototype/hott_fragment_v2.schema | awk '{print $1}')
header_fingerprint=$(awk '
	/PROTOTYPE_HOTT_CALCULUS_FINGERPRINT/ {
		getline
		gsub(/[\"\\]/, "")
		gsub(/[[:space:]]/, "")
		print
	}
' src/prototype/calculus.h)
if [ "$manifest_fingerprint" != "$header_fingerprint" ]; then
	echo "HOTT calculus fingerprint does not match hott_fragment_v2.schema" >&2
	exit 1
fi
changed_fingerprint=$(
	{ cat src/prototype/hott_fragment_v2.schema; printf '\nsemantic-change\n'; } |
		sha256sum | awk '{print $1}'
)
if [ "$changed_fingerprint" = "$header_fingerprint" ]; then
	echo "HOTT semantic change did not invalidate the fingerprint" >&2
	exit 1
fi

cc -std=c11 -Wall -Wextra -Werror -I src/prototype \
	src/prototype/hott_goal_check.c \
	src/prototype/hott.c \
	src/prototype/context.c \
	src/prototype/term.c \
	src/prototype/type_declaration.c \
	src/prototype/typing.c \
	src/prototype/universe.c \
	src/prototype/symbol.c \
	-o /tmp/a-program-hott-goal-check

/tmp/a-program-hott-goal-check
rm -f /tmp/a-program-hott-goal-check
