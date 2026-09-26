#!/bin/sh
set -eu

tmp_dir=${TMPDIR:-/tmp}/a-program-public-header-check
mkdir -p "$tmp_dir"

find src/prototype/include/a_program -type f -name '*.h' | sort |
while IFS= read -r header; do
	relative=${header#src/prototype/include/}
	printf '#include "%s"\nint main(void) { return 0; }\n' "$relative" \
		>"$tmp_dir/check.c"
	cc -std=c11 -Wall -Wextra -Werror \
		-I src/prototype/include -I src/prototype \
		-fsyntax-only "$tmp_dir/check.c"
done

printf '%s\n' 'public header self-containment tests passed'
