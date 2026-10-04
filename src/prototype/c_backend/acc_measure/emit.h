#ifndef __ACC_MEASURE_EMIT_H__
#define __ACC_MEASURE_EMIT_H__

#include "../indexed_views/emit.h"
#include "evidence.h"

struct pg_c_acc_measure_sources {
	const struct pg_occurrence *measure, *measure_reference, *outer, *outer_reference;
	const struct pg_occurrence *measure_generic, *accessibility, *sort_generic, *comparison;
};

/* Private actual-source measure/outer experiment; unsupported shapes preserve
	* the stream. Final-copy I/O and product cleanup remain caller-owned. */
int pg_c_acc_measure_emit(FILE *, struct pg_graph *, const struct pg_typing *,
	const struct pg_c_acc_measure_sources *, const struct pg_c_indexed_entry *);

#endif
