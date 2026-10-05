#include "observe.h"
#include "scope.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct snapshot_record {
	struct snapshot_record *next;
	const void *source;
	size_t bytes;
	unsigned char saved[];
};

struct pg_c_source_snapshot {
	struct snapshot_record *records;
	const struct pg_graph *graph;
	const struct pg_typing *typing;
	struct pg_graph saved_graph;
	struct pg_typing saved_typing;
};

static int save(struct pg_c_source_snapshot *snapshot, const void *source,
	size_t bytes)
{
	if (bytes>SIZE_MAX-sizeof(struct snapshot_record)) return -1;
	struct snapshot_record *record=malloc(sizeof(*record)+bytes);
	if (!record) return -1;
	record->next=snapshot->records; record->source=source; record->bytes=bytes;
	if (bytes) memcpy(record->saved,source,bytes);
	snapshot->records=record; return 0;
}

enum owner_kind { INDEX_ONLY, CONTEXT, SCOPE, OCCURRENCE, MAP };

static int save_index(struct pg_c_source_snapshot *snapshot,
	const struct pg_index *index, enum owner_kind kind)
{
	if (index->capacity>SIZE_MAX/sizeof(*index->buckets) ||
		save(snapshot,index->buckets,index->capacity*sizeof(*index->buckets))) return -1;
	for (size_t i=0; i<index->capacity; ++i)
		for (const struct pg_index_entry *entry=index->buckets[i]; entry; entry=entry->next) {
			size_t bytes=sizeof(*entry),extra=0;
			switch (kind) {
			case INDEX_ONLY: break;
			case CONTEXT: bytes=sizeof(struct pg_context); break;
			case SCOPE: bytes=sizeof(struct pg_scope); break;
			case OCCURRENCE: {
				const struct pg_occurrence *owner=(const void *)entry;
				bytes=sizeof(*owner);
				if (owner->operand_count>SIZE_MAX-owner->map_count) return -1;
				extra=owner->operand_count+owner->map_count;
				break;
			}
			case MAP: {
				const struct pg_context_map *owner=(const void *)entry;
				bytes=sizeof(*owner); extra=owner->count; break;
			}
			}
			if (extra>(SIZE_MAX-bytes)/sizeof(void *)) return -1;
			if (save(snapshot,entry,bytes+extra*sizeof(void *))) return -1;
		}
	return 0;
}

struct pg_c_source_snapshot *pg_c_source_snapshot_create(
	const struct pg_graph *graph, const struct pg_typing *typing)
{
	if (!graph || !typing || typing->graph!=graph) return NULL;
	struct pg_c_source_snapshot *snapshot=calloc(1,sizeof(*snapshot));
	if (!snapshot) return NULL;
	snapshot->graph=graph; snapshot->typing=typing;
	memcpy(&snapshot->saved_graph,graph,sizeof(*graph));
	memcpy(&snapshot->saved_typing,typing,sizeof(*typing));
	/* The entire container captures all counts and cache roots, including the
		* old producer's receipt index where present. Native owner payloads retain
		* admission marks, so admitting an existing tuple is observed too. */
	if (save_index(snapshot,&graph->terms,INDEX_ONLY) ||
		save_index(snapshot,&graph->objects,INDEX_ONLY) ||
		save_index(snapshot,&typing->contexts,CONTEXT) ||
		save_index(snapshot,&typing->scopes,SCOPE) ||
		save_index(snapshot,&typing->occurrences,OCCURRENCE) ||
		save_index(snapshot,&typing->context_maps,MAP) ||
		save_index(snapshot,&typing->context_projections,INDEX_ONLY) ||
		save_index(snapshot,&typing->context_lifts,INDEX_ONLY) ||
		save_index(snapshot,&typing->occurrence_actions,INDEX_ONLY) ||
		save_index(snapshot,&typing->occurrence_inputs,INDEX_ONLY) ||
		save_index(snapshot,&typing->typed_queries,INDEX_ONLY) ||
		save_index(snapshot,&typing->induction_requests,INDEX_ONLY)) {
		pg_c_source_snapshot_destroy(snapshot); return NULL;
	}
	return snapshot;
}

int pg_c_source_snapshot_unchanged(const struct pg_c_source_snapshot *snapshot)
{
	if (!snapshot) return 0;
	/* Check containers before borrowed bucket arrays: index growth may have
		* released an old array, while its stable graph owners remain alive. */
	if (memcmp(snapshot->graph,&snapshot->saved_graph,sizeof(*snapshot->graph)) ||
		memcmp(snapshot->typing,&snapshot->saved_typing,sizeof(*snapshot->typing))) return 0;
	for (const struct snapshot_record *record=snapshot->records; record; record=record->next)
		if (record->bytes && memcmp(record->source,record->saved,record->bytes)) return 0;
	return 1;
}

void pg_c_source_snapshot_destroy(struct pg_c_source_snapshot *snapshot)
{
	if (!snapshot) return;
	while (snapshot->records) {
		struct snapshot_record *record=snapshot->records;
		snapshot->records=record->next; free(record);
	}
	free(snapshot);
}
