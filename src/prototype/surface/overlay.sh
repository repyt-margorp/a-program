#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
overlay=${1:?usage: overlay.sh NEW_DIRECTORY}
export ARTIFACT_SOURCE=${ARTIFACT_SOURCE:-$(cd "$here/../.." && pwd)}
bash "$here/../solver_inputs/overlay.sh" "$overlay"
for change in "$here"/*.patch; do
	name=${change##*/}
	name=${name%.patch}
	if [[ -L "$overlay/src/$name" ]]; then
		cp --remove-destination "$(readlink -f "$overlay/src/$name")" "$overlay/src/$name"
	fi
	git apply --unsafe-paths --directory="$overlay/src" "$change"
done
for change in "$here"/test_patches/*.patch; do
	git apply --unsafe-paths --directory="$overlay/tests" "$change"
done
while IFS=$'\t' read -r source clauses old_hash new_hash; do
	[[ $source != source ]] || continue
	[[ $(sha256sum "$overlay/$source" | cut -d ' ' -f 1) == "$old_hash" ]] || {
		printf 'migration input changed: %s\n' "$source" >&2
		exit 1
	}
	[[ $(sha256sum "$here/migrated/$source" | cut -d ' ' -f 1) == "$new_hash" ]]
	cp "$here/migrated/$source" "$overlay/$source"
done < "$here/migration.tsv"
