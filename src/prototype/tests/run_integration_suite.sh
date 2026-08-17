#!/bin/sh
set -u

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
test_directory="$root_dir/src/prototype/tests/integration"
test_name_filter=
keep_going=0
timing_output=

usage() {
	cat <<'EOF'
usage: run_integration_suite.sh [--keep-going] [--timing-output FILE]
                                [--test-directory DIRECTORY]
                                [--test-name NAME]

Test stdout and stderr remain visible. Versioned timing records and the final
human-readable table are written to stderr. --timing-output copies only the
versioned records to FILE.
EOF
}

while [ "$#" -gt 0 ]; do
	case $1 in
	--keep-going)
		keep_going=1
		shift
		;;
	--timing-output)
		if [ "$#" -lt 2 ]; then
			usage >&2
			exit 2
		fi
		timing_output=$2
		shift 2
		;;
	--test-directory)
		if [ "$#" -lt 2 ]; then
			usage >&2
			exit 2
		fi
		test_directory=$2
		shift 2
		;;
	--test-name)
		if [ "$#" -lt 2 ]; then
			usage >&2
			exit 2
		fi
		test_name_filter=$2
		shift 2
		;;
	--help)
		usage
		exit 0
		;;
	*)
		usage >&2
		exit 2
		;;
	esac
done

if [ ! -d "$test_directory" ]; then
	echo "integration test directory does not exist: $test_directory" >&2
	exit 2
fi

runner_tmp=$(mktemp -d "${TMPDIR:-/tmp}/a-program-test-runner.XXXXXX") || exit 2
clock_binary="$runner_tmp/monotonic-clock"
results_file="$runner_tmp/results.tsv"
child_pid=
interrupted_exit=0

cleanup() {
	if [ -n "$child_pid" ]; then
		kill "$child_pid" 2>/dev/null || true
	fi
	rm -rf "$runner_tmp"
}

forward_signal() {
	signal_name=$1
	interrupted_exit=$2
	if [ -n "$child_pid" ]; then
		kill "-$signal_name" "$child_pid" 2>/dev/null || true
	fi
}

trap cleanup EXIT
trap 'forward_signal HUP 129' HUP
trap 'forward_signal INT 130' INT
trap 'forward_signal TERM 143' TERM

if ! cc -std=c11 -Wall -Wextra -Werror \
	"$root_dir/src/prototype/tests/support/monotonic_clock.c" \
	-o "$clock_binary"; then
	echo "failed to build the monotonic test clock" >&2
	exit 2
fi

now_milliseconds() {
	"$clock_binary"
}

emit_record() {
	record=$1
	printf '%s\n' "$record" >&2
	if [ -n "$timing_output" ]; then
		printf '%s\n' "$record" >>"$timing_output"
	fi
}

if [ -n "$timing_output" ]; then
	case $timing_output in
	/*) ;;
	*) timing_output=$(pwd)/$timing_output ;;
	esac
	: >"$timing_output" || exit 2
fi
: >"$results_file" || exit 2

if [ -n "$test_name_filter" ]; then
	case $test_name_filter in
	*[!A-Za-z0-9_.-]*|''|*.sh)
		echo "invalid integration test name: $test_name_filter" >&2
		exit 2
		;;
	esac
	set -- "$test_directory/$test_name_filter.sh"
	if [ ! -e "$1" ]; then
		echo "integration test not found: $test_name_filter" >&2
		exit 2
	fi
else
	set -- "$test_directory"/test_*.sh
fi
if [ "$1" = "$test_directory/test_*.sh" ] || [ ! -e "$1" ]; then
	echo "no integration tests found in: $test_directory" >&2
	exit 2
fi

discovered=0
seen_labels=' '
for test_script do
	if [ ! -f "$test_script" ] || [ ! -r "$test_script" ]; then
		echo "invalid integration test entry: $test_script" >&2
		exit 2
	fi
	test_file=$(basename -- "$test_script")
	test_name=${test_file%.sh}
	case $test_name in
	*[!A-Za-z0-9_.-]*|'')
		echo "invalid integration test label: $test_name" >&2
		exit 2
		;;
	esac
	case $seen_labels in
	*" $test_name "*)
		echo "duplicate integration test label: $test_name" >&2
		exit 2
		;;
	esac
	seen_labels="$seen_labels$test_name "
	discovered=$((discovered + 1))

	if tr -d "\"'" <"$test_script" | grep -Eq \
		'^[[:space:]]*((sh|\.)[[:space:]]+)?[^[:space:]]*tests/integration/test_[A-Za-z0-9_.-]*\.sh([[:space:]]*\\)?[[:space:]]*$|^[[:space:]]*(sh|\.)[[:space:]]+(\./)?test_[A-Za-z0-9_.-]*\.sh([[:space:]]*\\)?[[:space:]]*$'; then
		echo "nested integration test invocation: $test_script" >&2
		exit 2
	fi
done

suite_start=$(now_milliseconds) || exit 2
executed=0
passed=0
failed=0
fast=0
normal=0
slow=0
very_slow=0
first_failure=0

for test_script do
	test_file=$(basename -- "$test_script")
	test_name=${test_file%.sh}
	printf '==> %s\n' "$test_script"
	test_start=$(now_milliseconds) || exit 2
	phase_state="$runner_tmp/phase-$executed.tsv"
	: >"$phase_state"
	A_PROGRAM_TEST_CLOCK=$clock_binary A_PROGRAM_TEST_NAME=$test_name \
		A_PROGRAM_TEST_PHASE_STATE=$phase_state \
		A_PROGRAM_TEST_TIMING_OUTPUT=$timing_output \
		A_PROGRAM_TEST_SUPPORT="$root_dir/src/prototype/build/test_support.sh" \
		sh "$test_script" &
	child_pid=$!
	wait "$child_pid"
	test_exit=$?
	child_pid=
	if [ "$interrupted_exit" -ne 0 ]; then
		test_exit=$interrupted_exit
	fi
	test_end=$(now_milliseconds) || exit 2
	wall_ms=$((test_end - test_start))
	if [ "$wall_ms" -lt 0 ]; then
		echo "monotonic clock moved backwards" >&2
		exit 2
	fi
	if [ -s "$phase_state" ]; then
		IFS="$(printf '\t')" read -r phase_start phase_name <"$phase_state"
		phase_wall_ms=$((test_end - phase_start))
		if [ "$phase_wall_ms" -lt 0 ]; then
			echo "monotonic phase clock moved backwards" >&2
			exit 2
		fi
		if [ "$test_exit" -eq 0 ]; then
			phase_status=pass
		elif [ "$test_exit" -gt 128 ]; then
			phase_status=signal
		else
			phase_status=fail
		fi
		emit_record "A_PROGRAM_TEST_PHASE 1 suite=integration test=$test_name phase=$phase_name status=$phase_status exit=$test_exit wall_ms=$phase_wall_ms"
	fi

	executed=$((executed + 1))
	if [ "$test_exit" -eq 0 ]; then
		status=pass
		passed=$((passed + 1))
	elif [ "$test_exit" -gt 128 ]; then
		status=signal
		failed=$((failed + 1))
	else
		status=fail
		failed=$((failed + 1))
	fi

	if [ "$wall_ms" -lt 1000 ]; then
		fast=$((fast + 1))
	elif [ "$wall_ms" -lt 10000 ]; then
		normal=$((normal + 1))
	elif [ "$wall_ms" -lt 60000 ]; then
		slow=$((slow + 1))
	else
		very_slow=$((very_slow + 1))
	fi

	emit_record "A_PROGRAM_TEST_TIMING 1 suite=integration test=$test_name status=$status exit=$test_exit wall_ms=$wall_ms"
	printf '%s\t%s\t%s\t%s\n' \
		"$wall_ms" "$test_name" "$status" "$test_exit" >>"$results_file"

	if [ "$test_exit" -ne 0 ]; then
		if [ "$first_failure" -eq 0 ]; then
			first_failure=$test_exit
		fi
		if [ "$keep_going" -eq 0 ] || [ "$interrupted_exit" -ne 0 ]; then
			break
		fi
	fi
done

suite_end=$(now_milliseconds) || exit 2
suite_wall_ms=$((suite_end - suite_start))
skipped=$((discovered - executed))
emit_record "A_PROGRAM_TEST_SUMMARY 1 suite=integration total=$discovered executed=$executed passed=$passed failed=$failed skipped=$skipped wall_ms=$suite_wall_ms"
emit_record "A_PROGRAM_TEST_BANDS 1 suite=integration fast=$fast normal=$normal slow=$slow very_slow=$very_slow"

printf '%-10s  %-44s  %-7s  %s\n' "WALL_MS" "TEST" "STATUS" "EXIT" >&2
sort -nr -k1,1 "$results_file" | while IFS="$(printf '\t')" read -r wall name state code; do
	printf '%10s  %-44s  %-7s  %s\n' "$wall" "$name" "$state" "$code" >&2
done

if [ "$interrupted_exit" -ne 0 ]; then
	exit "$interrupted_exit"
fi
if [ "$first_failure" -ne 0 ]; then
	if [ "$keep_going" -ne 0 ]; then
		exit 1
	fi
	exit "$first_failure"
fi
exit 0
