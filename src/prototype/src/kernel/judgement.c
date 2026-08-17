#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"
#include "a_program/dimension/action.h"
#include "a_program/dimension/operator.h"
#include "a_program/graph/typed_occurrence_graph.h"
#include "a_program/graph/occurrence_usage.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keep the kernel in one translation unit while exposing physical ownership. */
#include "typing/judgement_db.inc"
#include "typing/conversion.inc"
#include "typing/classifier_solver.inc"
#include "typing/candidate_publication.inc"
#include "rules/formation_early.inc"
#include "rules/introduction_lambda.inc"
#include "rules/elimination_app.inc"
#include "rules/cbpv.inc"
#include "rules/match.inc"
#include "rules/formation_host.inc"
#include "rules/introduction_identity.inc"
#include "rules/introduction/dimension_action.inc"
#include "rules/formation_recording.inc"
#include "typing/candidate_replay.inc"
#include "typing/accepted_replay.inc"
