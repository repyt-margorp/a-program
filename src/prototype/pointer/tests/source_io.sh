#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" write "$directory/modules.graph"
single=$("$1" read "$directory/modules.graph")
bulk=$("$1" read-bulk "$directory/modules.graph")
test "$single" = "$bulk"
printf '%s\n' "$single"
