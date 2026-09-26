#!/bin/sh
set -eu

# This is an inventory of the frozen implementation, not a passing-test list.
baseline=5bdecb4
{
printf 'category\tname\tsource\tstatus\n'
git show "$baseline:src/prototype/include/a_program/frontend/ast.h" |
	awk '/^[ \t]*PROTOTYPE_AST_[A-Z_]+[ ,=]/ {
		name=$1; sub(/[,=].*$/, "", name);
		print "ast\t" name "\tsrc/prototype/include/a_program/frontend/ast.h\treview_pending"
	}'
for source in src/prototype/src/driver/read_file.c src/prototype/src/driver/repl.c
do
	git show "$baseline:$source" |
		awk -v source="$source" -F '"' '{
			for (i=2; i<=NF; i+=2)
				if ($i ~ /^--[a-z][a-z-]*$/)
					print "cli\t" $i "\t" source "\treview_pending"
		}' | sort -u
done
git ls-tree -r --name-only "$baseline" |
	awk '
		/\.p$/ { print "program\t" $0 "\t" $0 "\treview_pending"; next }
		/src\/prototype\/tests\/integration\/.*\.sh$/ {
			print "integration\t" $0 "\t" $0 "\treview_pending"; next
		}
		/src\/prototype\/tests\/checks\/.*\.(c|inc)$/ {
			print "internal_check\t" $0 "\t" $0 "\treview_pending"
		}'
} | awk -F '\t' 'BEGIN { OFS = FS }
	{ sub(/^src\/prototype\//, "archive/legacy/src/prototype/", $3); print }'
