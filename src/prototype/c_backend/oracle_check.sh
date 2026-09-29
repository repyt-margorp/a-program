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
printf 'C Oracle differential: Int32/Int64 wrapping/formatting and nominal multi-clause handling passed\n'
