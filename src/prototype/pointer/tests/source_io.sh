#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" context-scopes
"$1" constructor-inputs
"$1" match-origins
"$1" handler-scopes
"$1" write "$directory/modules.graph"
single=$("$1" read "$directory/modules.graph")
bulk=$("$1" read-bulk "$directory/modules.graph")
test "$single" = "$bulk"
printf '%s\n' "$single"
retained_case() {
	local binary=$1 writer=$2 fixture=$3 reader failed=0
	local path="$directory/$writer-$fixture"
	printf 'retention: %s %s\n' "$writer" "$fixture"
	"$binary" "$writer" "$path.a" "$fixture" || return 1
	"$binary" retained-resave "$path.a" "$path-again.a" || return 1
	"$binary" retained-resave "$path-again.a" "$path-final.a" || return 1
	for reader in retained-check retained-recompute; do
		if ! "$binary" "$reader" "$path-final.a"; then
			printf 'FAIL: %s %s %s\n' "$writer" "$fixture" "$reader" >&2
			failed=1
		fi
	done
	return "$failed"
}
failed=0
for writer in retained-write-typed retained-write; do
	for fixture in lambda nominal nullary application constructor match; do
		if ! retained_case "$1" "$writer" "$fixture"; then
			printf 'FAIL: retention case %s %s\n' "$writer" "$fixture" >&2
			failed=1
		fi
	done
done
"$1" annotation-write "$directory/annotations.graph"
single=$("$1" annotation-read "$directory/annotations.graph")
bulk=$("$1" annotation-read-bulk "$directory/annotations.graph")
test "$single" = "$bulk"
printf '%s\n' "$single"
for writer in module-annotation-write module-annotation-write-settled; do
	"$1" "$writer" "$directory/module-annotations.graph"
	single=$("$1" module-annotation-read "$directory/module-annotations.graph")
	bulk=$("$1" module-annotation-read-bulk "$directory/module-annotations.graph")
	test "$single" = "$bulk"
	printf '%s\n' "$single"
done
"$1" nominal-write "$directory/nominal.graph"
single=$("$1" nominal-read "$directory/nominal.graph")
bulk=$("$1" nominal-read-bulk "$directory/nominal.graph")
test "$single" = "$bulk"
printf '%s\n' "$single"
exit "$failed"
