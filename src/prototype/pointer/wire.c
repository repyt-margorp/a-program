#include "wire.h"

int pg_wire_write_u64(FILE *file, uint64_t value)
{
	unsigned char bytes[8];
	for (size_t i = 0; i < 8; ++i) bytes[i] = (unsigned char)(value >> (8 * i));
	return fwrite(bytes, 1, 8, file) == 8 ? 0 : -1;
}

int pg_wire_read_u64(FILE *file, uint64_t *value)
{
	unsigned char bytes[8];
	if (fread(bytes, 1, 8, file) != 8) return -1;
	*value = 0;
	for (size_t i = 0; i < 8; ++i) *value |= (uint64_t)bytes[i] << (8 * i);
	return 0;
}
