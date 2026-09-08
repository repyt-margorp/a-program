#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" write "$directory/derivations.graph"
single=$("$1" read "$directory/derivations.graph")
bulk=$("$1" read-bulk "$directory/derivations.graph")
test "$single" = "$bulk"
"$1" operation-write "$directory/operations.graph"
single_operations=$("$1" operation-read "$directory/operations.graph")
bulk_operations=$("$1" operation-read-bulk "$directory/operations.graph")
test "$single_operations" = "$bulk_operations"
printf '%s\n' "$single_operations"
"$1" input-write "$directory/inputs.graph"
single_inputs=$("$1" input-read "$directory/inputs.graph")
bulk_inputs=$("$1" input-read-bulk "$directory/inputs.graph")
test "$single_inputs" = "$bulk_inputs"
printf '%s\n' "$single_inputs"
printf '%s\n' "$single" 'derivation io: shared premises, split-budget Solve, receipt recovery and rejection passed'
