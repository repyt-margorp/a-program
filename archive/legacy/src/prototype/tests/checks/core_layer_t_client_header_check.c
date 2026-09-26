#include "a_program/protocol/calculation.h"

#include <stdint.h>

/* This translation unit models the Core surface visible to Layer T. It must
 * compile without term.h, CorePipeline, provider representation, or TermDB
 * storage. */
int main(void) {
	struct prototype_core_inspect_request inspect_request = { 0 };
	struct prototype_core_inspect_response inspect_response = { 0 };
	struct prototype_core_normalization_request request = { 0 };
	struct prototype_core_normalization_response response = { 0 };
	struct prototype_term term = { 0 };

	(void)request;
	(void)response;
	(void)inspect_request;
	(void)inspect_response;
	(void)term;
	return 0;
}
