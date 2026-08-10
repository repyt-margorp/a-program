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

	# Source groups contain repository-controlled paths and are intentionally
	# split into shell words for the compiler invocation.
	# shellcheck disable=SC2046
	cc "-std=$standard" -Wall -Wextra $werror -I src/prototype \
		"$@" $(prototype_source_group "$group") -o "$output"
}

