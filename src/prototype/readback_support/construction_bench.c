#include "graph.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	if (argc != 2) return 2;
	char *end;
	unsigned long count = strtoul(argv[1], &end, 10);
	if (*end || !count || count > 10000) return 2;
	clock_t started = clock();
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_object **binders = malloc(count * sizeof(*binders));
	assert(binders);
	const struct pg_term *term = NULL;
	for (unsigned long i = 0; i < count; ++i) {
		binders[i] = pg_binder(&graph);
		const struct pg_term *variable = pg_reference(&graph, binders[i]);
		term = term ? pg_application(&graph, term, variable) : variable;
		assert(term);
	}
	for (unsigned long i = count; i; --i) {
		term = pg_lambda(&graph, binders[i - 1], term);
		assert(term);
	}
	printf("binders=%lu terms=%zu\n", count, graph.terms.count);
	FILE *memory = fopen("/proc/self/statm", "r");
	unsigned long virtual_pages, resident_pages;
	assert(memory && fscanf(memory, "%lu %lu", &virtual_pages, &resident_pages) == 2);
	fclose(memory);
	printf("cpu-seconds=%.6f current-rss-kib=%lu\n",
		(double)(clock() - started) / CLOCKS_PER_SEC,
		resident_pages * (unsigned long)sysconf(_SC_PAGESIZE) / 1024);
	free(binders);
	pg_graph_destroy(&graph);
	return 0;
}
