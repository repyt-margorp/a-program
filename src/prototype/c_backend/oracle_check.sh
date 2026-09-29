#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"${1:?Oracle generator}" "$directory/oracles.c" "$directory/expected"
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
"${CC:-cc}" "${flags[@]}" -I"$here" "$directory/oracles.c" "$here/runtime.c" -o "$directory/run"
"$directory/run" > "$directory/actual"
cmp "$directory/expected" "$directory/actual"
for source in "$directory"/oracles.c.fail-*.c; do
	"${CC:-cc}" "${flags[@]}" -I"$here" "$source" "$here/runtime.c" -o "$directory/run"
	status=0
	"$directory/run" > "$directory/actual" 2> "$directory/diagnostic" || status=$?
	test "$status" = 4
	test ! -s "$directory/actual"
	grep -q 'unsupported Identity family field' "$directory/diagnostic"
done
printf 'C Oracle differential: Identity centers/maps, neutral refusal, host arithmetic and multi-clause handling passed\n'
