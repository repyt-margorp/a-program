#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" write "$directory/derivations.graph"
single=$("$1" read "$directory/derivations.graph")
bulk=$("$1" read-bulk "$directory/derivations.graph")
test "$single" = "$bulk"
for grade in '' total-; do
	"$1" "operation-${grade}write" "$directory/operations.graph"
	single_operations=$("$1" "operation-${grade}read" "$directory/operations.graph")
	bulk_operations=$("$1" "operation-${grade}read-bulk" "$directory/operations.graph")
	test "$single_operations" = "$bulk_operations"
	printf '%s\n' "$single_operations"
done
"$1" input-write "$directory/inputs.graph"
single_inputs=$("$1" input-read "$directory/inputs.graph")
bulk_inputs=$("$1" input-read-bulk "$directory/inputs.graph")
test "$single_inputs" = "$bulk_inputs"
printf '%s\n' "$single_inputs"
"$1" effect-write "$directory/effects.graph"
single_effects=$("$1" effect-read "$directory/effects.graph")
bulk_effects=$("$1" effect-read-bulk "$directory/effects.graph")
test "$single_effects" = "$bulk_effects"
printf '%s\n' "$single_effects"
"$1" producer-write "$directory/producers.graph"
single_producers=$("$1" producer-read "$directory/producers.graph")
bulk_producers=$("$1" producer-read-bulk "$directory/producers.graph")
test "$single_producers" = "$bulk_producers"
printf '%s\n' "$single_producers"
"$1" nominal-write "$directory/nominal.graph"
single_nominal=$("$1" nominal-read "$directory/nominal.graph")
bulk_nominal=$("$1" nominal-read-bulk "$directory/nominal.graph")
test "$single_nominal" = "$bulk_nominal"
printf '%s\n' "$single_nominal"
printf '%s\n' "$single" 'derivation io: shared premises, split-budget Solve, receipt recovery and rejection passed'
