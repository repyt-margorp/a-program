#include "seed.h"
#include "syntax_io.h"
#include "source_io.h"
#include "descriptor_io.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const char source[] = "{{ id:=&(\\A:@ => \\x:A => x); id::(A:@)->A->A; }}.id";

static void compare(struct pg_program *loaded, const char *text, enum pg_definition_policy policy)
{
	struct pg_program *fresh = pg_program_create(text, strlen(text), policy);
	FILE *file = tmpfile();
	assert(file && !pg_seed_write(file, text, strlen(text), policy));
	rewind(file);
	struct pg_program *batch = pg_seed_read(file, 4096);
	assert(batch && !fclose(file));
	assert(loaded && fresh && loaded->root && fresh->root);
	assert(!loaded->parser.reader.input && fresh->parser.reader.input);
	assert(loaded->synthesis.definition_policy == policy);
	assert(!loaded->synthesis.steps && !fresh->synthesis.steps);
	assert(pg_synthesis_status(loaded->root) == PG_SYNTHESIS_PENDING);
	assert(!pg_synthesis_result(loaded->root));
	for (size_t i = 0; pg_synthesis_status(loaded->root) == PG_SYNTHESIS_PENDING; ++i) {
		assert(i < 10000);
		pg_synthesis_advance(&loaded->synthesis, 1);
	}
	pg_synthesis_advance(&fresh->synthesis, 10000);
	pg_synthesis_advance(&batch->synthesis, 10000);
	assert(pg_synthesis_status(loaded->root) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(fresh->root) == PG_SYNTHESIS_DONE);
	/* Image namespace derivations require ordinary Solve work beyond source setup. */
	assert(pg_synthesis_status(batch->root) == PG_SYNTHESIS_DONE);
	assert(loaded->synthesis.steps == batch->synthesis.steps);
	assert(pg_alpha_equal(pg_evidence_subject(pg_synthesis_result(loaded->root))->core,
		pg_evidence_subject(pg_synthesis_result(batch->root))->core) == 1);
	assert(pg_alpha_equal(pg_evidence_subject(pg_synthesis_result(loaded->root))->core,
		pg_evidence_subject(pg_synthesis_result(fresh->root))->core) == 1);
	/* Universe/row descriptors belong to separate arenas. Compare their
	 * transported structure, not the source pointers of those references. */
	FILE *left = tmpfile(), *right = tmpfile();
	const struct pg_term *a = pg_evidence_classifier(pg_synthesis_result(loaded->root));
	const struct pg_term *b = pg_evidence_classifier(pg_synthesis_result(fresh->root));
	assert(left && right);
	assert(!pg_graph_write_descriptors(left, 1, &a, &pg_builtin_graph_codec, &loaded->graph));
	assert(!pg_graph_write_descriptors(right, 1, &b, &pg_builtin_graph_codec, &fresh->graph));
	rewind(left); rewind(right);
	int byte;
	do { byte = fgetc(left); assert(byte == fgetc(right)); } while (byte != EOF);
	assert(!ferror(left) && !ferror(right));
	assert(!fclose(left) && !fclose(right));
	pg_program_destroy(batch);
	pg_program_destroy(fresh);
	pg_program_destroy(loaded);
}

static struct pg_program *read_bytes(const unsigned char *bytes, size_t length, size_t limit)
{
	FILE *file = tmpfile();
	assert(file && fwrite(bytes, 1, length, file) == length);
	rewind(file);
	struct pg_program *result = pg_seed_read(file, limit);
	assert(fclose(file) == 0);
	return result;
}

int main(int argc, char **argv)
{
	if (argc == 3) {
		if (!strcmp(argv[1], "write")) {
			FILE *file = fopen(argv[2], "wb");
			assert(file && pg_seed_write(file, source, strlen(source), PG_DEFINITION_EXPLICIT_THUNK) == 0);
			assert(fclose(file) == 0);
		} else {
			assert(!strcmp(argv[1], "read"));
			FILE *file = fopen(argv[2], "rb");
			assert(file);
			struct pg_program *program = pg_seed_read(file, 4096);
			assert(fclose(file) == 0);
			compare(program, source, PG_DEFINITION_EXPLICIT_THUNK);
		}
		return 0;
	}
	assert(argc == 1);
	for (int policy = PG_DEFINITION_IMPLICIT_THUNK; policy <= PG_DEFINITION_EXPLICIT_THUNK; ++policy) {
		FILE *file = tmpfile();
		assert(file && pg_seed_write(file, source, strlen(source), policy) == 0);
		long end = ftell(file);
		assert(end > 0 && (uintmax_t)end < SIZE_MAX);
		size_t length = (size_t)end;
		unsigned char *bytes = malloc(length + 1);
		assert(bytes);
		rewind(file);
		assert(fread(bytes, 1, length, file) == length && fgetc(file) == EOF);
		assert(!ferror(file) && fclose(file) == 0);
		assert(length > 56 && !memcmp(bytes, "APGSRC\74", 8));
		assert(bytes[8] == policy);
		compare(read_bytes(bytes, length, 4096), source, policy);
		for (unsigned version = 34; version <= 45; ++version) {
			bytes[6] = version;
			assert(!read_bytes(bytes, length, 4096));
		}
		bytes[6] = 46;
		assert(!read_bytes(bytes, length, 0));
		for (size_t cut = 0; cut < length; ++cut) assert(!read_bytes(bytes, cut, 4096));
		bytes[length] = 0;
		assert(!read_bytes(bytes, length + 1, 4096));
		bytes[8] = 2;
		assert(!read_bytes(bytes, length, 4096));
		bytes[8] = 0;
		bytes[6] = 0;
		assert(!read_bytes(bytes, length, 4096));
		bytes[6] = 7;
		assert(!read_bytes(bytes, length, 4096));
		bytes[6] = 26;
		assert(!read_bytes(bytes, length, 4096));
		bytes[6] = 46;
		memset(bytes + 16, 255, 8);
		assert(!read_bytes(bytes, length, 4096));
		free(bytes);
	}
	FILE *file = tmpfile();
	assert(file && pg_seed_write(file, "x:=", 3, PG_DEFINITION_EXPLICIT_THUNK) == -1);
	assert(fclose(file) == 0);
	/* Structurally decoded nodes are not automatically admissible programs. */
	file = tmpfile();
	struct pg_program *p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
	assert(file && p);
	const struct pg_syntax malformed = {.kind = PG_SYNTAX_APPLICATION};
	struct pg_synthesis_job *root = pg_synthesis_request(&p->synthesis, p->scope, &malformed);
	assert(root && !pg_sources_write(file, &p->synthesis, 1, &root));
	rewind(file);
	assert(!pg_seed_read(file, 4096));
	assert(!fclose(file));
	pg_program_destroy(p);
	assert(pg_seed_write(NULL, source, strlen(source), PG_DEFINITION_EXPLICIT_THUNK) == -1);
	assert(!pg_seed_read(NULL, 4096));
	puts("seed: unresolved syntax graph, exact policy, fresh solve without reparsing and input validation passed");
	return 0;
}
