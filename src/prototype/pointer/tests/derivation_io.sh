#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" write "$directory/derivations.graph"
single=$("$1" read "$directory/derivations.graph")
bulk=$("$1" read-bulk "$directory/derivations.graph")
test "$single" = "$bulk"
printf '%s\n' "$single" 'derivation io: shared premises, split-budget Solve, receipt recovery and rejection passed'
