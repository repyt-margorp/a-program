#!/usr/bin/env bash
set -eu
binary=$1
shift
if [ "$#" -eq 0 ]; then
	printf '%s\n' 'no source examples supplied' >&2
	exit 2
fi
failed=0
total=$#
for source in "$@"; do
	code=0
	output=$("$binary" --steps 1000000 "$source" 2>&1) || code=$?
	printf '%s\texit=%s\t%s\n' "$source" "$code" "$output"
	if [ "$code" -ne 0 ]; then
		failed=$((failed + 1))
	fi
done
printf 'source checking: %s/%s passed (not runtime result validation)\n' "$((total - failed))" "$total"
test "$failed" -eq 0
