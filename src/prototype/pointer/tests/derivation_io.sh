#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" write "$directory/derivations.graph"
"$1" read "$directory/derivations.graph"
printf '%s\n' 'derivation io: shared premises, fresh ordinary acceptance and recomputed normalization passed'
