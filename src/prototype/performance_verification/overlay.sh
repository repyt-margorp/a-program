#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
overlay=${1:?usage: overlay.sh NEW_DIRECTORY}
bash "$here/../performance/overlay.sh" "$overlay" head-adapted
# Apply generated prototype patches to private copies, never accepted symlinks.
if [[ -L "$overlay/src/eval.c" ]]; then
	cp --remove-destination "$(readlink -f "$overlay/src/eval.c")" "$overlay/src/eval.c"
fi
apply_once() {
	local directory=$1 patch=$2
	if git apply --reverse --check --unsafe-paths --directory="$directory" "$patch" 2>/dev/null; then
		return
	fi
	git apply --check --unsafe-paths --directory="$directory" "$patch"
	git apply --unsafe-paths --directory="$directory" "$patch"
}
apply_once "$overlay/src" "$here/eval_frame_cleanup.patch"
apply_once "$overlay/tests" "$here/identity_io_adapter.patch"
# Accepted head tests need the prototype codec's additional link dependencies.
if rg -q '^HEAD_MACHINE_IO :=' "$overlay/src/Makefile"; then
	printf '\nHEAD_MACHINE_IO += $(filter-out $(HEAD_MACHINE_IO) $(ROOT)main.c,$(CLI_SOURCES)) $(ROOT)artifact/schedule.c\n' >> "$overlay/src/Makefile"
fi
printf '%s\n' head-adapted-cleanup > "$overlay/variant.txt"
sha256sum "$here/eval_frame_cleanup.patch" "$here/identity_io_adapter.patch" \
	> "$overlay/corrective-patches.sha256"
find "$overlay/src" -maxdepth 1 -name '*.[ch]' -print0 |
	sort -z | xargs -0 sha256sum > "$overlay/source-sha256.txt"
