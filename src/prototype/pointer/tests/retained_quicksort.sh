#!/usr/bin/env bash
set -euo pipefail
binary=$1
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
failed=0
for mode in solved retained whnf retained-whnf; do
	options=()
	case "$mode" in retained|retained-whnf) options+=(--retain-reductions);; esac
	case "$mode" in whnf|retained-whnf) options+=(--whnf main);; esac
	"$binary" --steps 1000000 --legacy-intrinsic-dot "${options[@]}" \
		--save "$directory/$mode.a" \
		--imports "$root/../tests/fixtures/typing/if8_fuel_free_quicksort_check.p" \
		"$root/tests/acceptance/legacy-quicksort-property.p" > "$directory/save"
	if "$binary" --steps 1000000 --load "$directory/$mode.a" > "$directory/load"; then
		printf '%s: ' "$mode"
		cat "$directory/load"
	else
		printf '%s: saved valid source failed to reload\n' "$mode" >&2
		cat "$directory/load" >&2
		failed=1
	fi
done
exit "$failed"
