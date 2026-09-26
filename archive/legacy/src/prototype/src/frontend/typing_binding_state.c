#include "a_program/frontend/typing_binding_state.h"

#include <string.h>

void prototype_typing_binding_state_init(
	struct prototype_typing_binding_state* state
) {
	if (!state) return;
	memset(state, 0, sizeof(*state));
}

int prototype_typing_context_classifier_view_init(
	const struct prototype_context_db* contexts,
	struct prototype_context_classifier_view* view
) {
	if (!contexts || !view) {
		return -1;
	}
	*view = (struct prototype_context_classifier_view) {
		.contexts = contexts
	};
	return 0;
}
