#!/bin/sh
set -eu

tmp_dir=${TMPDIR:-/tmp}/a-program-source-manifest-check
mkdir -p "$tmp_dir"

for group in kernel graph compiler hott repl reader; do
	make -s -f src/prototype/Makefile "print-$group-sources" >"$tmp_dir/$group.raw"
	LC_ALL=C sort "$tmp_dir/$group.raw" >"$tmp_dir/$group.sorted"
	LC_ALL=C sort -u "$tmp_dir/$group.raw" >"$tmp_dir/$group.unique"
	cmp "$tmp_dir/$group.sorted" "$tmp_dir/$group.unique"
done

require_subset() {
	missing=$(comm -23 "$tmp_dir/$1.unique" "$tmp_dir/$2.unique")
	if [ -n "$missing" ]; then
		printf 'source group %s is not a subset of %s:\n%s\n' "$1" "$2" "$missing" >&2
		exit 1
	fi
}

require_subset kernel graph
require_subset kernel compiler
require_subset kernel hott
require_subset graph compiler
require_subset compiler repl
require_subset compiler reader

make -s -f src/prototype/Makefile reader
make -f src/prototype/Makefile -pn a.out >"$tmp_dir/make-database"
grep '^.*/src/prototype/src/kernel/judgement\.o:.*src/prototype/src/kernel/typing/accepted_replay\.inc' \
	"$tmp_dir/make-database" >/dev/null
make -f src/prototype/Makefile -pn read_file.out >"$tmp_dir/reader-make-database"
grep '^read_file\.out:.*.*/src/prototype/src/driver/read_file\.o' \
	"$tmp_dir/reader-make-database" >/dev/null
grep '^.*/src/prototype/src/driver/read_file\.o:.*src/prototype/src/driver/read_file\.c' \
	"$tmp_dir/reader-make-database" >/dev/null

printf '%s\n' 'source manifest set tests passed'
