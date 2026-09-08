#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" write "$directory/inputs.graph"
"$1" read "$directory/inputs.graph"
printf '%s\n' 'occurrence io: shared Core, distinct annotations, ordered operands and zero evidence passed'
