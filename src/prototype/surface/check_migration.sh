#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd "$here/../../.." && pwd)
migrator=${1:?usage: check_migration.sh MIGRATOR}
scratch=$(mktemp -d /tmp/ap-surface-migration.XXXXXX)
trap 'rm -rf "$scratch"' EXIT
while IFS=$'\t' read -r source clauses old_hash new_hash; do
	[[ $source != source ]] || continue
	[[ $(sha256sum "$repo/$source" | cut -d ' ' -f 1) == "$old_hash" ]]
	[[ $(sha256sum "$here/migrated/$source" | cut -d ' ' -f 1) == "$new_hash" ]]
	"$migrator" --legacy-source "$repo/$source" > "$scratch/migrated.p" 2> "$scratch/report"
	cmp "$scratch/migrated.p" "$here/migrated/$source"
	rg -q "^migrated $clauses named clauses$" "$scratch/report"
done < "$here/migration.tsv"
printf '%s\n' '// @fake { lhs := rhs; }' 'x := #"@fake { lhs := rhs; }";' \
	'm := g @c { /* keep := this */ source := local; same; } => local;' > "$scratch/comments.p"
"$migrator" --legacy-source "$scratch/comments.p" > "$scratch/comments-new.p"
printf '%s\n' '// @fake { lhs := rhs; }' 'x := #"@fake { lhs := rhs; }";' \
	'm := g @c {{ /* keep := this */ local := source; same; }} => local;' > "$scratch/expected.p"
cmp "$scratch/comments-new.p" "$scratch/expected.p"
printf '%s\n' 'm := { x := #"text"; x; };' > "$scratch/plain.p"
"$migrator" --legacy-source "$scratch/plain.p" > "$scratch/plain-new.p"
cmp "$scratch/plain.p" "$scratch/plain-new.p"
code=0
"$migrator" "$scratch/comments.p" > "$scratch/unflagged.p" 2>&1 || code=$?
[[ $code == 2 ]]
"$migrator" --legacy-source "$scratch/comments-new.p" > "$scratch/twice.p" 2>&1 && exit 1
printf '%s\n' 'surface migration: eight pinned consumers, 13 clauses, token-preserving comments/text, explicit legacy input passed'
