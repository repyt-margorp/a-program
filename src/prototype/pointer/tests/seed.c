#include "seed.h"

#include <assert.h>
#include <string.h>

static const char source[] = "{{ id:=&(\\A:@ => \\x:A => x); id::(A:@)->A->A; }}.id";

static void compare(struct pg_program *loaded, const char *text, enum pg_definition_policy policy)
{
	struct pg_program *fresh = pg_program_create(text, strlen(text), policy);
	assert(loaded && fresh && loaded->root && fresh->root);
	assert(loaded->synthesis.definition_policy == policy);
	assert(!loaded->synthesis.steps && !fresh->synthesis.steps);
	assert(pg_synthesis_status(loaded->root) == PG_SYNTHESIS_PENDING);
	assert(!pg_synthesis_result(loaded->root));
	for (size_t i = 0; pg_synthesis_status(loaded->root) == PG_SYNTHESIS_PENDING; ++i) {
		assert(i < 10000);
		pg_synthesis_advance(&loaded->synthesis, 1);
	}
	pg_synthesis_advance(&fresh->synthesis, 10000);
	assert(pg_synthesis_status(loaded->root) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(fresh->root) == PG_SYNTHESIS_DONE);
	assert(loaded->synthesis.steps == fresh->synthesis.steps);
	assert(pg_alpha_equal(pg_evidence_subject(pg_synthesis_result(loaded->root))->core,
		pg_evidence_subject(pg_synthesis_result(fresh->root))->core) == 1);
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
	unsigned char bytes[4096];
	for (int policy = PG_DEFINITION_IMPLICIT_THUNK; policy <= PG_DEFINITION_EXPLICIT_THUNK; ++policy) {
		FILE *file = tmpfile();
		assert(file && pg_seed_write(file, source, strlen(source), policy) == 0);
		rewind(file);
		size_t length = fread(bytes, 1, sizeof(bytes), file);
		assert(feof(file) && !ferror(file) && fclose(file) == 0);
		assert(length == 17 + strlen(source));
		assert(!memcmp(bytes, "APGSEED\0", 8));
		assert(bytes[8] == (policy == PG_DEFINITION_EXPLICIT_THUNK ? 0 : 1));
		assert(bytes[9] == strlen(source));
		for (size_t i = 10; i < 17; ++i) assert(bytes[i] == 0);
		assert(!memcmp(bytes + 17, source, strlen(source)));
		compare(read_bytes(bytes, length, strlen(source)), source, policy);
		assert(!read_bytes(bytes, length, strlen(source) - 1));
		for (size_t cut = 0; cut < length; ++cut) assert(!read_bytes(bytes, cut, 4096));
		bytes[length] = 0;
		assert(!read_bytes(bytes, length + 1, 4096));
		bytes[8] = 2;
		assert(!read_bytes(bytes, length, 4096));
		bytes[8] = 0;
		bytes[7] = 1;
		assert(!read_bytes(bytes, length, 4096));
		bytes[7] = 0;
		memset(bytes + 9, 255, 8);
		assert(!read_bytes(bytes, length, 4096));
	}
	FILE *file = tmpfile();
	assert(file && pg_seed_write(file, "x:=", 3, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	rewind(file);
	struct pg_program *invalid = pg_seed_read(file, 3);
	assert(invalid && !invalid->root && invalid->parser.error && !invalid->synthesis.steps);
	pg_program_destroy(invalid);
	assert(fclose(file) == 0);
	assert(pg_seed_write(NULL, source, strlen(source), PG_DEFINITION_EXPLICIT_THUNK) == -1);
	assert(!pg_seed_read(NULL, 4096));
	puts("seed: ordinary unresolved creation, exact policy, fresh solve and input validation passed");
	return 0;
}
