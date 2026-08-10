#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

prototype_compile c11 werror compiler \
	/tmp/a-program-term-identity-frame-check \
	src/prototype/term_identity_frame_check.c

/tmp/a-program-term-identity-frame-check
rm -f /tmp/a-program-term-identity-frame-check
