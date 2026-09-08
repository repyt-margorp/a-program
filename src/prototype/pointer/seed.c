#include "seed.h"

#include <stdlib.h>
#include <string.h>

static const unsigned char magic[8] = {'A', 'P', 'G', 'S', 'E', 'E', 'D', 0};

int pg_seed_write(FILE *file, const char *source, size_t length,
	enum pg_definition_policy policy)
{
	if (!file || (!source && length)) return -1;
	unsigned char header[17] = {0};
	memcpy(header, magic, sizeof(magic));
	switch (policy) {
	case PG_DEFINITION_EXPLICIT_THUNK: header[8] = 0; break;
	case PG_DEFINITION_IMPLICIT_THUNK: header[8] = 1; break;
	default: return -1;
	}
	uint64_t size = length;
	if ((size_t)size != length) return -1;
	for (size_t i = 0; i < 8; ++i) header[9 + i] = (unsigned char)(size >> (8 * i));
	if (fwrite(header, 1, sizeof(header), file) != sizeof(header)) return -1;
	if (length && fwrite(source, 1, length, file) != length) return -1;
	return ferror(file) ? -1 : 0;
}

struct pg_program *pg_seed_read(FILE *file, size_t limit)
{
	if (!file) return NULL;
	unsigned char header[17];
	if (fread(header, 1, sizeof(header), file) != sizeof(header)) return NULL;
	if (memcmp(header, magic, sizeof(magic))) return NULL;
	enum pg_definition_policy policy;
	switch (header[8]) {
	case 0: policy = PG_DEFINITION_EXPLICIT_THUNK; break;
	case 1: policy = PG_DEFINITION_IMPLICIT_THUNK; break;
	default: return NULL;
	}
	uint64_t size = 0;
	for (size_t i = 0; i < 8; ++i) size |= (uint64_t)header[9 + i] << (8 * i);
	if (size > SIZE_MAX || size > limit) return NULL;
	size_t length = (size_t)size;
	char *source = malloc(length ? length : 1);
	if (!source) return NULL;
	struct pg_program *program = NULL;
	if (length && fread(source, 1, length, file) != length) goto done;
	if (fgetc(file) != EOF || ferror(file)) goto done;
	program = pg_program_create(source, length, policy);
done:
	free(source);
	return program;
}
