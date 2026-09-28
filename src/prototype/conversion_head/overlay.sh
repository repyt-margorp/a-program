#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source=$(cd "$here/../.." && pwd)
overlay=${1:?usage: overlay.sh BUILD_DIRECTORY}
bash "$here/../readback_support/overlay.sh" "$overlay"
patch --silent --fuzz=0 --output "$overlay/src/conversion.c.new" "$source/conversion.c" "$here/conversion.c.patch"
mv -f "$overlay/src/conversion.c.new" "$overlay/src/conversion.c"
