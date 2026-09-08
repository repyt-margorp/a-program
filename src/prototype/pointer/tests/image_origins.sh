#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" origin-write "$directory/origins.a"
"$1" origin-read "$directory/origins.a"
