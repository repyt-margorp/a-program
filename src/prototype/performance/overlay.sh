#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source=$(cd "$here/../.." && pwd)
overlay=${1:?usage: overlay.sh NEW_DIRECTORY [base|direct|head|head-adapted]}
variant=${2:-direct}
case $variant in base|direct|head|head-adapted) ;; *) exit 2;; esac
# Accepted source and the prerequisite overlays must match the pinned worktree.
git -C "$source/.." diff --quiet HEAD -- src ':!src/prototype'
git -C "$source/.." diff --quiet HEAD -- src/prototype/solver_inputs src/prototype/artifact_persistence src/prototype/readback_support src/prototype/conversion_head
# Always build this worktree's committed producer, never Main's dirty source.
ARTIFACT_SOURCE="$source" bash "$here/../solver_inputs/overlay.sh" "$overlay"
if [[ $variant != base ]]; then
	if [[ -L "$overlay/src/iadt.c" ]]; then
		cp --remove-destination "$(readlink -f "$overlay/src/iadt.c")" "$overlay/src/iadt.c"
	fi
	git apply --check --unsafe-paths --directory="$overlay/src" "$here/iadt.c.patch"
	git apply --unsafe-paths --directory="$overlay/src" "$here/iadt.c.patch"
fi
if [[ $variant == head || $variant == head-adapted ]]; then
	for name in eval.c eval.h computation.c; do
		if [[ -L "$overlay/src/$name" ]]; then
			cp --remove-destination "$(readlink -f "$overlay/src/$name")" "$overlay/src/$name"
		fi
		git apply --check --unsafe-paths --directory="$overlay/src" "$here/$name.patch"
		git apply --unsafe-paths --directory="$overlay/src" "$here/$name.patch"
	done
	git apply --check --unsafe-paths --directory="$overlay/src" "$here/iadt_head.c.patch"
	git apply --unsafe-paths --directory="$overlay/src" "$here/iadt_head.c.patch"
fi
if [[ $variant == head-adapted ]]; then
	git apply --check --unsafe-paths --directory="$overlay/tests" "$here/core_adapter.patch"
	git apply --unsafe-paths --directory="$overlay/tests" "$here/core_adapter.patch"
fi
git -C "$source/.." rev-parse HEAD > "$overlay/revision.txt"
printf '%s\n' "$variant" > "$overlay/variant.txt"
find "$overlay/src" -maxdepth 1 -name '*.[ch]' -print0 |
	sort -z | xargs -0 sha256sum > "$overlay/source-sha256.txt"
