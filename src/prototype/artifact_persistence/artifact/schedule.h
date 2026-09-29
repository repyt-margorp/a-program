#ifndef A_PROGRAM_ARTIFACT_SCHEDULE_H
#define A_PROGRAM_ARTIFACT_SCHEDULE_H

#include "synthesis.h"
#include <stdio.h>

struct pg_artifact_schedule;

/* Scheduler portion of a checkpoint. The enclosing owner supplies the same
 * ordered job mapping on both sides, including every ready/wait endpoint.
 * Job identities, private continuation state and evidence are NOT encoded here
 * and must already be restored under the enclosing provenance policy.
 * Reading creates inert transport data owned by storage, not runnable jobs.
 * Attachment is separate: it replaces the schedule only after all mappings
 * validate, preserving FIFO ready order and child wake order/preparation.
 * There is no accepted-status import or fuel charge. Failed attachment leaves
 * the old schedule unchanged (unused arena allocations may remain).
 * The enclosing reader must not publish a Program before all payloads validate.
 * Initial reconstruction may leave a request queued and subscribed, or an owner
 * may complete it before startup dispatch. Replacement tolerates these transient
 * states; saved ready/wait endpoints must still be disjoint and pending.
 * Dormant jobs remain dormant; other jobs may exist only outside the schedule. */
int pg_artifact_schedule_write(FILE *, const struct pg_synthesis *, size_t,
	struct pg_synthesis_job *const *);
const struct pg_artifact_schedule *pg_artifact_schedule_read(FILE *, struct pg_graph *storage, size_t limit);
int pg_artifact_schedule_save(FILE *, const struct pg_artifact_schedule *);
int pg_artifact_schedule_attach(struct pg_synthesis *, const struct pg_artifact_schedule *, size_t,
	struct pg_synthesis_job *const *);

#endif
