#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source=$(cd "$here/../.." && pwd)
overlay=${1:?usage: overlay.sh BUILD_DIRECTORY}
bash "$here/../conversion_head/overlay.sh" "$overlay"
patch --silent --fuzz=0 --output "$overlay/src/main.c.new" "$source/main.c" "$here/main.c.patch"
mv -f "$overlay/src/main.c.new" "$overlay/src/main.c"
