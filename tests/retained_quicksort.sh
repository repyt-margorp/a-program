#!/usr/bin/env bash
set -euo pipefail
binary=$1
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
failed=0
# Includes execution of the explicit ordinary-result proof, not just sorting.
steps=5000000
for mode in solved retained whnf retained-whnf; do
	options=()
	case "$mode" in retained|retained-whnf) options+=(--retain-reductions);; esac
	case "$mode" in whnf|retained-whnf) options+=(--whnf main);; esac
	"$binary" --steps "$steps" --legacy-intrinsic-dot "${options[@]}" \
		--save "$directory/$mode.a" \
		--imports "$root/archive/legacy/src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p" \
		"$root/tests/acceptance/legacy-quicksort-property.p" > "$directory/save"
	if "$binary" --steps "$steps" --load "$directory/$mode.a" > "$directory/load"; then
		printf '%s: ' "$mode"
		cat "$directory/load"
	else
		printf '%s: saved valid source failed to reload\n' "$mode" >&2
		cat "$directory/load" >&2
		failed=1
	fi
	if [[ $mode == whnf || $mode == retained-whnf ]]; then
		for root_index in 1 2; do
			if ! "$binary" --steps "$steps" --load --root "$root_index" \
				"$directory/$mode.a" > "$directory/root-load"; then
				printf '%s root %s failed to reload\n' "$mode" "$root_index" >&2
				cat "$directory/root-load" >&2
				failed=1
			fi
		done
	fi
	if [[ $mode == retained-whnf ]]; then
		code=0
		"$binary" --load --steps 0 --save "$directory/resaved.a" --retain-reductions \
			"$directory/$mode.a" > "$directory/resave" || code=$?
		if [[ $code != 3 ]] || ! grep -qx 'pending steps=0' "$directory/resave" ||
			! cmp -s "$directory/$mode.a" "$directory/resaved.a"; then
			printf '%s inert resave changed the image\n' "$mode" >&2
			failed=1
		fi
	fi
done
exit "$failed"
