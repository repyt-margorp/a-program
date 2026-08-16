#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

prototype_compile c11 werror compiler \
	/tmp/a-program-typed-occurrence-transaction-check \
	src/prototype/tests/checks/typed_occurrence_transaction_check.c

/tmp/a-program-typed-occurrence-transaction-check
rm -f /tmp/a-program-typed-occurrence-transaction-check
