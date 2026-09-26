#!/usr/bin/env bash
set -euo pipefail
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
"$1" write "$scratch/typed.a"
"$1" read "$scratch/typed.a"
