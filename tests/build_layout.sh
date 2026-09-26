#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
runner=${1:-make}
for entry in Makefile src/Makefile; do
	"$runner" --no-print-directory -s -Bn -f "$root/$entry" \
		BUILD="$directory/build" pointer-check > "$directory/${entry//\//-}"
done
cmp "$directory/Makefile" "$directory/src-Makefile"
cd "$directory"
"$runner" --no-print-directory -s -Bn -f "$root/Makefile" \
	BUILD="$directory/build" > default
cmp Makefile default
grep -Fq -- "$root/src/main.c" default
for excluded in archive src/prototype src/handmade; do
	if grep -Fq -- "$root/$excluded/" default; then
		printf '%s\n' 'current build unexpectedly includes archived or handmade code' >&2
		exit 1
	fi
done
printf '%s\n' 'build layout: root/direct/default entry points use only the current compiler'
