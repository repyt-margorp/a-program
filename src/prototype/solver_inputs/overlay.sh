#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
overlay=${1:?usage: overlay.sh NEW_DIRECTORY}
bash "$here/../artifact_persistence/candidate.sh" "$overlay"
for patch in "$here"/*.patch; do
	name=${patch##*/}
	name=${name%.patch}
	if [[ -L "$overlay/src/$name" ]]; then
		cp --remove-destination "$(readlink -f "$overlay/src/$name")" "$overlay/src/$name"
	fi
	git apply --unsafe-paths --directory="$overlay/src" "$patch"
done
for patch in "$here"/test_patches/*.patch; do
	git apply --unsafe-paths --directory="$overlay/tests" "$patch"
done
