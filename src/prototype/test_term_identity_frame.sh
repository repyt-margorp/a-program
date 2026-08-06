#!/bin/sh
set -eu

cc -std=c11 -Wall -Wextra -Werror -I src/prototype \
	src/prototype/term_identity_frame_check.c \
	src/prototype/ast.c \
	src/prototype/ast_inspect.c \
	src/prototype/reader.c \
	src/prototype/term.c \
	src/prototype/type_declaration.c \
	src/prototype/typing.c \
	src/prototype/universe.c \
	src/prototype/symbol.c \
	-o /tmp/a-program-term-identity-frame-check

/tmp/a-program-term-identity-frame-check
rm -f /tmp/a-program-term-identity-frame-check
