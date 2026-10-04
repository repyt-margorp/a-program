#!/usr/bin/env bash
set -euo pipefail
pointer=${C_CAPTURE_POINTER:?exact qualified pointer-check}
commands=${C_CAPTURE_COMMANDS:?retained transformed argv}
arguments=("$@")
for i in "${!arguments[@]}"; do
	if [[ ${arguments[i]} == --save ]]; then arguments[i]=--save-materialized; fi
done
# Current accepted --save defaults to recomputable inputs, without completed
# trusted exports. This test adapter chooses the existing materialized profile.
printf '%s\0' "$pointer" "${#arguments[@]}" "${arguments[@]}" >>"$commands"
exec "$pointer" "${arguments[@]}"
