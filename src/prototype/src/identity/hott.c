#include "a_program/frontend/lowering.h"
#include "a_program/identity/types.h"
#include "a_program/identity/telescope_action.h"
#include "a_program/identity/context_bridge.h"
#include "a_program/identity/relation_action.h"
#include "a_program/identity/action_certificate.h"
#include "a_program/identity/identity_computation.h"
#include "a_program/identity/object_term_action.h"
#include "a_program/identity/action_execution.h"
#include "calculus.h"

#include <stdlib.h>
#include <string.h>

/* Preserve one identity-action translation unit while exposing ownership. */
#include "relation_action.inc"
#include "action_certificate_init.inc"
#include "artifact_root_extraction.inc"
#include "action_certificate_validation.inc"
#include "telescope_action.inc"
#include "identity_computation.inc"
#include "context_bridge.inc"
#include "object_term_action.inc"
#include "action_execution.inc"
