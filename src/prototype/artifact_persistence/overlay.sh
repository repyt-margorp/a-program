#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source=${ARTIFACT_SOURCE:-$(cd "$here/../.." && pwd)}
overlay=${1:?usage: overlay.sh NEW_DIRECTORY}
mkdir "$overlay"
mkdir "$overlay/src"
for input in "$source"/*.[ch] "$source/Makefile"; do
	name=${input##*/}
	if [[ -f "$here/$name.patch" ]]; then
		cp "$input" "$overlay/src/$name"
		git apply --unsafe-paths --directory="$overlay/src" "$here/$name.patch"
	else
		ln -s "$input" "$overlay/src/$name"
	fi
done
repo=$(cd "$source/.." && pwd)
for name in tests examples archive training; do ln -s "$repo/$name" "$overlay/$name"; done
ln -s "$repo/print.p" "$repo/Makefile" "$overlay/"
