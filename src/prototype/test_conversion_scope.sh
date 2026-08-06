#!/bin/sh
set -eu

if rg -n 'static int match_frame_keys_equal' src/prototype/term.c; then
	echo "kernel conversion still has key-only Match-frame equality" >&2
	exit 1
fi

cc -std=c11 -Wall -Wextra -Werror -I src/prototype \
	src/prototype/conversion_scope_check.c \
	src/prototype/ast.c \
	src/prototype/context.c \
	src/prototype/reader.c \
	src/prototype/term.c \
	src/prototype/type_declaration.c \
	src/prototype/typing.c \
	src/prototype/universe.c \
	src/prototype/symbol.c \
	-o /tmp/a-program-conversion-scope-check

/tmp/a-program-conversion-scope-check
rm -f /tmp/a-program-conversion-scope-check
