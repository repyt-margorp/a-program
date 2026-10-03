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
apply_once() {
	local directory=$1 patch=$2
	if git apply --reverse --check --unsafe-paths --directory="$directory" "$patch" 2>/dev/null; then
		return
	fi
	git apply --check --unsafe-paths --directory="$directory" "$patch"
	git apply --unsafe-paths --directory="$directory" "$patch"
}
head_eval_is_accepted() {
	local cleanup="$here/../performance_verification/eval_frame_cleanup.patch"
	local scratch
	git apply --reverse --check --unsafe-paths --directory="$overlay/src" "$cleanup" 2>/dev/null || return 1
	scratch=$(mktemp -d)
	cp "$overlay/src/eval.c" "$scratch/eval.c"
	# Validate the two exact patches on a copy; retain accepted cleanup in the overlay.
	if git apply --reverse --unsafe-paths --directory="$scratch" "$cleanup" &&
		git apply --reverse --check --unsafe-paths --directory="$scratch" "$here/eval.c.patch" 2>/dev/null; then
		rm -rf "$scratch"
		return 0
	fi
	rm -rf "$scratch"
	return 1
}
if [[ $variant != base ]]; then
	if [[ -L "$overlay/src/iadt.c" ]]; then
		cp --remove-destination "$(readlink -f "$overlay/src/iadt.c")" "$overlay/src/iadt.c"
	fi
	apply_once "$overlay/src" "$here/iadt.c.patch"
fi
if [[ $variant == head || $variant == head-adapted ]]; then
	for name in eval.c eval.h computation.c; do
		if [[ -L "$overlay/src/$name" ]]; then
			cp --remove-destination "$(readlink -f "$overlay/src/$name")" "$overlay/src/$name"
		fi
		if [[ $name == eval.c ]] && head_eval_is_accepted; then
			continue
		fi
		apply_once "$overlay/src" "$here/$name.patch"
	done
	apply_once "$overlay/src" "$here/iadt_head.c.patch"
fi
if [[ $variant == head-adapted ]]; then
	apply_once "$overlay/tests" "$here/core_adapter.patch"
fi
git -C "$source/.." rev-parse HEAD > "$overlay/revision.txt"
printf '%s\n' "$variant" > "$overlay/variant.txt"
find "$overlay/src" -maxdepth 1 -name '*.[ch]' -print0 |
	sort -z | xargs -0 sha256sum > "$overlay/source-sha256.txt"
