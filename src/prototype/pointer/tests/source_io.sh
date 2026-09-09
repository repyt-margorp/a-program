#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" write "$directory/modules.graph"
single=$("$1" read "$directory/modules.graph")
bulk=$("$1" read-bulk "$directory/modules.graph")
test "$single" = "$bulk"
printf '%s\n' "$single"
"$1" annotation-write "$directory/annotations.graph"
single=$("$1" annotation-read "$directory/annotations.graph")
bulk=$("$1" annotation-read-bulk "$directory/annotations.graph")
test "$single" = "$bulk"
printf '%s\n' "$single"
"$1" nominal-write "$directory/nominal.graph"
single=$("$1" nominal-read "$directory/nominal.graph")
bulk=$("$1" nominal-read-bulk "$directory/nominal.graph")
test "$single" = "$bulk"
printf '%s\n' "$single"
