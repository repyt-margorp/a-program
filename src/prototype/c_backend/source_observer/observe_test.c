#include "observe.h"
#include "evidence.h"
#include "scope.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
	assert(argc==1 || (argc==2 && !strcmp(argv[1],"--native")));
	struct pg_graph graph; struct pg_typing typing;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing,&graph));
	if (argc==2) assert(pg_scope_intern(&typing,NULL,NULL,NULL,NULL));
	size_t occurrences=typing.occurrences.count,contexts=typing.contexts.count,
		scopes=typing.scopes.count,maps=typing.context_maps.count;
	struct pg_c_source_snapshot *snapshot=pg_c_source_snapshot_create(&graph,&typing);
	assert(snapshot && pg_c_source_snapshot_unchanged(snapshot));
	/* On the native producer this admits its already interned empty Scope.
		* Its descriptive counts remain fixed; the admission mark must be seen. */
	assert(pg_prove_empty_context(&typing));
	assert(typing.occurrences.count==occurrences && typing.contexts.count==contexts &&
		typing.scopes.count==scopes && typing.context_maps.count==maps);
	assert(!pg_c_source_snapshot_unchanged(snapshot)); pg_c_source_snapshot_destroy(snapshot);
	snapshot=pg_c_source_snapshot_create(&graph,&typing);
	assert(snapshot && pg_c_source_snapshot_unchanged(snapshot));
	assert(pg_prove_empty_context(&typing) && pg_c_source_snapshot_unchanged(snapshot));
	pg_c_source_snapshot_destroy(snapshot);
	/* Pre-intern the exact Universe conclusion. Native admission changes its
		* owner payload without changing any container, index or Core counts. */
	const struct pg_occurrence *subject=pg_occurrence(&typing,PG_JUDGEMENT_VALUE_TYPE,NULL,
		pg_universe(&graph,3),pg_universe(&graph,4),NULL,0,NULL); assert(subject);
	snapshot=pg_c_source_snapshot_create(&graph,&typing);
	assert(snapshot && pg_c_source_snapshot_unchanged(snapshot));
	unsigned char before[sizeof(typing)]; memcpy(before,&typing,sizeof(typing));
	const struct pg_evidence *context=pg_prove_empty_context(&typing);
	assert(pg_evidence_subject(pg_prove_universe(&typing,context,3))==subject);
	if (argc==2) assert(!memcmp(before,&typing,sizeof(typing)));
	assert(!pg_c_source_snapshot_unchanged(snapshot)); pg_c_source_snapshot_destroy(snapshot);
	snapshot=pg_c_source_snapshot_create(&graph,&typing);
	assert(snapshot && pg_c_source_snapshot_unchanged(snapshot));
	assert(pg_reference(&graph,pg_binder(&graph)));
	assert(!pg_c_source_snapshot_unchanged(snapshot)); pg_c_source_snapshot_destroy(snapshot);
	pg_typing_destroy(&typing); pg_graph_destroy(&graph);
	puts("observer: stable read/repeated admission pass; existing Scope/typed admission and new Core detected");
	return 0;
}
