prototype_source_group() {
	make -s -f src/prototype/Makefile "print-$1-sources"
}

prototype_compile() {
	standard=$1
	warnings=$2
	group=$3
	output=$4
	shift 4

	werror=
	if [ "$warnings" = "werror" ]; then
		werror=-Werror
	fi
	compile_flags="-std=$standard -Wall -Wextra"
	if [ -n "$werror" ]; then
		compile_flags="$compile_flags $werror"
	fi

	local_sources=
	extra_cppflags=
	for argument do
		case $argument in
		*.c)
			local_sources="$local_sources $argument"
			;;
		*)
			extra_cppflags="$extra_cppflags $argument"
			;;
		esac
	done
	compiler_identity=$(cc --version | sed -n '1p')
	configuration=$(printf '%s\n' \
		"compiler=$compiler_identity cppflags=-I src/prototype/include -I src/prototype$extra_cppflags cflags=$compile_flags" |
		sha256sum | awk '{ print $1 }')
	object_root=${A_PROGRAM_TEST_OBJECT_ROOT:-${XDG_CACHE_HOME:-${TMPDIR:-/tmp}}/a-program-prototype-test-objects}
	object_root=$object_root/$configuration
	production_sources=$(prototype_source_group "$group")
	warning_flags="-Wall -Wextra $werror"
	make -s -f src/prototype/build/test_objects.mk prototype-test-objects \
		TEST_STANDARD="$standard" \
		TEST_WARNING_FLAGS="$warning_flags" \
		TEST_CPPFLAGS="$extra_cppflags" \
		TEST_OBJECT_ROOT="$object_root" \
		TEST_SOURCES="$production_sources"
	production_objects=$(make -s -f src/prototype/build/test_objects.mk \
		print-prototype-test-objects \
		TEST_OBJECT_ROOT="$object_root" TEST_SOURCES="$production_sources")

	local_object_directory=$output.objects
	rm -rf "$local_object_directory"
	mkdir -p "$local_object_directory"
	local_objects=
	for source in $local_sources; do
		object_name=$(printf '%s\n' "$source" | cksum |
			awk '{ print $1 "-" $2 ".o" }')
		object=$local_object_directory/$object_name
		# Test-local sources may intentionally share a basename.
		# shellcheck disable=SC2086
		cc "-std=$standard" -Wall -Wextra $werror $extra_cppflags \
			-I src/prototype/include -I src/prototype -c "$source" -o "$object"
		local_objects="$local_objects $object"
	done
	# Object paths are generated from repository-controlled paths without spaces.
	# shellcheck disable=SC2086
	cc $production_objects $local_objects -o "$output"
	rm -rf "$local_object_directory"
}

prototype_test_timing_initialize() {
	timing_directory=$1
	if [ -z "${A_PROGRAM_TEST_CLOCK:-}" ]; then
		A_PROGRAM_TEST_CLOCK="$timing_directory/monotonic-test-clock"
		cc -std=c11 -Wall -Wextra -Werror \
			src/prototype/tests/support/monotonic_clock.c \
			-o "$A_PROGRAM_TEST_CLOCK"
	fi
	if [ -z "${A_PROGRAM_TEST_NAME:-}" ]; then
		test_file=$(basename -- "$0")
		A_PROGRAM_TEST_NAME=${test_file%.sh}
	fi
	if [ -z "${A_PROGRAM_TEST_PHASE_STATE:-}" ]; then
		A_PROGRAM_TEST_PHASE_STATE="$timing_directory/test-phase.state"
	fi
	: >"$A_PROGRAM_TEST_PHASE_STATE"
	export A_PROGRAM_TEST_CLOCK A_PROGRAM_TEST_NAME A_PROGRAM_TEST_PHASE_STATE
}

prototype_test_timing_emit() {
	timing_record=$1
	printf '%s\n' "$timing_record" >&2
	if [ -n "${A_PROGRAM_TEST_TIMING_OUTPUT:-}" ]; then
		printf '%s\n' "$timing_record" >>"$A_PROGRAM_TEST_TIMING_OUTPUT"
	fi
}

prototype_test_phase() {
	next_phase=$1
	case $next_phase in
	*[!A-Za-z0-9_.-]*|'')
		echo "invalid test phase name: $next_phase" >&2
		return 2
		;;
	esac
	phase_now=$("$A_PROGRAM_TEST_CLOCK") || return 2
	if [ -s "$A_PROGRAM_TEST_PHASE_STATE" ]; then
		IFS="$(printf '\t')" read -r phase_start phase_name \
			<"$A_PROGRAM_TEST_PHASE_STATE"
		phase_wall_ms=$((phase_now - phase_start))
		if [ "$phase_wall_ms" -lt 0 ]; then
			echo 'monotonic test phase clock moved backwards' >&2
			return 2
		fi
		prototype_test_timing_emit \
			"A_PROGRAM_TEST_PHASE 1 suite=integration test=$A_PROGRAM_TEST_NAME phase=$phase_name status=pass exit=0 wall_ms=$phase_wall_ms"
	fi
	printf '%s\t%s\n' "$phase_now" "$next_phase" \
		>"$A_PROGRAM_TEST_PHASE_STATE"
}

prototype_test_phase_finish() {
	phase_now=$("$A_PROGRAM_TEST_CLOCK") || return 2
	if [ ! -s "$A_PROGRAM_TEST_PHASE_STATE" ]; then
		return 0
	fi
	IFS="$(printf '\t')" read -r phase_start phase_name \
		<"$A_PROGRAM_TEST_PHASE_STATE"
	phase_wall_ms=$((phase_now - phase_start))
	if [ "$phase_wall_ms" -lt 0 ]; then
		echo 'monotonic test phase clock moved backwards' >&2
		return 2
	fi
	prototype_test_timing_emit \
		"A_PROGRAM_TEST_PHASE 1 suite=integration test=$A_PROGRAM_TEST_NAME phase=$phase_name status=pass exit=0 wall_ms=$phase_wall_ms"
	: >"$A_PROGRAM_TEST_PHASE_STATE"
}
