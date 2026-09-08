#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" write "$directory/program.seed"
"$1" read "$directory/program.seed"
printf '%s\n' 'seed: separate-process RECOMPUTE passed (not CHECKPOINT)'
