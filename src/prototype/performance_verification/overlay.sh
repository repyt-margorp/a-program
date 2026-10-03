#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
overlay=${1:?usage: overlay.sh NEW_DIRECTORY}
bash "$here/../performance/overlay.sh" "$overlay" head-adapted
# Apply generated prototype patches to private copies, never accepted symlinks.
if [[ -L "$overlay/src/eval.c" ]]; then
	cp --remove-destination "$(readlink -f "$overlay/src/eval.c")" "$overlay/src/eval.c"
fi
git apply --check --unsafe-paths --directory="$overlay/src" "$here/eval_frame_cleanup.patch"
git apply --unsafe-paths --directory="$overlay/src" "$here/eval_frame_cleanup.patch"
git apply --check --unsafe-paths --directory="$overlay/tests" "$here/identity_io_adapter.patch"
git apply --unsafe-paths --directory="$overlay/tests" "$here/identity_io_adapter.patch"
printf '%s\n' head-adapted-cleanup > "$overlay/variant.txt"
sha256sum "$here/eval_frame_cleanup.patch" "$here/identity_io_adapter.patch" \
	> "$overlay/corrective-patches.sha256"
find "$overlay/src" -maxdepth 1 -name '*.[ch]' -print0 |
	sort -z | xargs -0 sha256sum > "$overlay/source-sha256.txt"
