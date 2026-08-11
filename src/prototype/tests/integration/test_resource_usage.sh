#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

prototype_compile c11 werror kernel \
	/tmp/a-program-resource-usage-check \
	src/prototype/tests/checks/resource_usage_check.c

/tmp/a-program-resource-usage-check
rm -f /tmp/a-program-resource-usage-check
