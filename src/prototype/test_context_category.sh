#!/bin/sh
set -eu

cc -std=c11 -Wall -Wextra -I src/prototype \
	src/prototype/context_category_check.c \
	src/prototype/ast.c \
	src/prototype/reader.c \
	src/prototype/term.c \
	src/prototype/type_declaration.c \
	src/prototype/typing.c \
	src/prototype/universe.c \
	src/prototype/symbol.c \
	-o /tmp/a-program-context-category-check

/tmp/a-program-context-category-check
rm -f /tmp/a-program-context-category-check
