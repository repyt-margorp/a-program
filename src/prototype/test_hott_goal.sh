#!/bin/sh
set -eu

cc -std=c11 -Wall -Wextra -Werror -I src/prototype \
	src/prototype/hott_goal_check.c \
	src/prototype/context.c \
	src/prototype/term.c \
	src/prototype/type_declaration.c \
	src/prototype/typing.c \
	src/prototype/universe.c \
	src/prototype/symbol.c \
	-o /tmp/a-program-hott-goal-check

/tmp/a-program-hott-goal-check
rm -f /tmp/a-program-hott-goal-check
