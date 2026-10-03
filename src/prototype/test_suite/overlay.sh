#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
overlay=${1:?usage: overlay.sh NEW_DIRECTORY [original|reduced]}
mode=${2:-reduced}
case $mode in original|reduced) ;; *) exit 2 ;; esac
bash "$here/../performance_verification/overlay.sh" "$overlay"
# Compose the committed Surface migration on the same E9/E10 producer.
for change in "$here/../surface/"*.patch "$here/../surface/test_patches/"*.patch; do
	directory=src
	if [[ $change == */test_patches/* ]]; then directory=tests; fi
	file="$overlay/$directory/$(basename "$change" .patch)"
	if [[ -L $file ]]; then cp --remove-destination "$(readlink -f "$file")" "$file"; fi
	git apply --unsafe-paths --directory="$overlay/$directory" "$change"
done
while IFS=$'\t' read -r source clauses old_hash new_hash; do
	[[ $source != source ]] || continue
	[[ $(sha256sum "$overlay/$source" | cut -d ' ' -f 1) == "$old_hash" ]]
	[[ $(sha256sum "$here/../surface/migrated/$source" | cut -d ' ' -f 1) == "$new_hash" ]]
	cp "$here/../surface/migrated/$source" "$overlay/$source"
done < "$here/../surface/migration.tsv"
if [[ $mode == reduced ]]; then
	git apply --check --unsafe-paths --directory="$overlay/tests" "$here/quick_result.sh.patch" "$here/compatibility.sh.patch"
	git apply --unsafe-paths --directory="$overlay/tests" "$here/quick_result.sh.patch" "$here/compatibility.sh.patch"
fi
