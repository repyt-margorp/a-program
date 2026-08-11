#!/bin/sh
set -eu

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

for source in \
	resumption_multiplicity_check \
	resumption_one_shot_branch_check \
	resumption_multi_shot_check \
	resumption_abortive_check
do
	./read_file.out "src/prototype/tests/fixtures/effects/$source.p" \
		>"$tmp_dir/$source.out"
	grep -q '^term main := ' "$tmp_dir/$source.out"
done

if ./read_file.out src/prototype/tests/fixtures/effects/resumption_one_shot_invalid.p \
	>"$tmp_dir/one-shot-invalid.out" 2>"$tmp_dir/one-shot-invalid.err"
then
	echo 'one-shot handler accepted two direct continuation uses' >&2
	exit 1
fi

if ./read_file.out src/prototype/tests/fixtures/effects/resumption_abortive_invalid.p \
	>"$tmp_dir/abortive-invalid.out" 2>"$tmp_dir/abortive-invalid.err"
then
	echo 'abortive handler accepted a continuation use' >&2
	exit 1
fi

for source in \
	resumption_multiplicity_check \
	resumption_one_shot_branch_check \
	resumption_multi_shot_check \
	resumption_abortive_check
do
	printf ':q\n' | ./a.out "src/prototype/tests/fixtures/effects/$source.p" \
		>"$tmp_dir/$source.eval"
done
grep -q '^value main := RETURN(TEXT_LITERAL("once"))$' \
	"$tmp_dir/resumption_multiplicity_check.eval"
grep -q '^value main := RETURN(TEXT_LITERAL("true"))$' \
	"$tmp_dir/resumption_one_shot_branch_check.eval"
grep -q '^value main := RETURN(TEXT_LITERAL("second"))$' \
	"$tmp_dir/resumption_multi_shot_check.eval"
grep -q '^value main := RETURN(TEXT_LITERAL("abort"))$' \
	"$tmp_dir/resumption_abortive_check.eval"

./read_file.out --write-artifact "$tmp_dir/multi.apo" \
	src/prototype/tests/fixtures/effects/resumption_multi_shot_check.p >"$tmp_dir/multi-write.out"
./read_file.out --read-graph "$tmp_dir/multi.apo" >"$tmp_dir/multi-read.out"
grep -q 'interface term main ' "$tmp_dir/multi-read.out"

echo 'resumption multiplicity tests passed'
