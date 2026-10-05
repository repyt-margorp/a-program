#ifndef __PG_C_SOURCE_OBSERVE_H__
#define __PG_C_SOURCE_OBSERVE_H__

struct pg_graph;
struct pg_typing;
struct pg_c_source_snapshot;

/* Target-side diagnostics only. Borrow the source until snapshot destruction.
	* Capture owner/index bytes without running checking or allocating in source. */
struct pg_c_source_snapshot *pg_c_source_snapshot_create(
	const struct pg_graph *, const struct pg_typing *);
int pg_c_source_snapshot_unchanged(const struct pg_c_source_snapshot *);
void pg_c_source_snapshot_destroy(struct pg_c_source_snapshot *);

#endif
