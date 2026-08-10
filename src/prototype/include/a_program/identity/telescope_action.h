#ifndef __A_PROGRAM_IDENTITY_TELESCOPE_ACTION_H__
#define __A_PROGRAM_IDENTITY_TELESCOPE_ACTION_H__

#include "a_program/identity/types.h"

int prototype_hott_type_former_descriptor_query(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_db* judgement,
	uint32_t source_claim_id,
	struct prototype_hott_type_former_descriptor* p_descriptor
);


#endif
