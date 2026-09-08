#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" write "$directory/core.graph"
"$1" read "$directory/core.graph"
"$1" operation-write "$directory/operations.graph"
"$1" operation-read "$directory/operations.graph"
"$1" effect-write "$directory/effects.graph"
"$1" effect-read "$directory/effects.graph"
"$1" effect-read-bulk "$directory/effects.graph"
printf '%s\n' 'graph acceptance: fresh-process relocation, shared Core and distinct typed evidence passed'
