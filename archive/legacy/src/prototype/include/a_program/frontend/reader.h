#ifndef __PROTOTYPE_READER_H__
#define __PROTOTYPE_READER_H__

struct prototype_program;

enum prototype_read_diagnostic_code {
	PROTOTYPE_READ_DIAGNOSTIC_NONE = 0,
	PROTOTYPE_READ_DIAGNOSTIC_SYNTAX = 1,
	PROTOTYPE_READ_DIAGNOSTIC_UNSUPPORTED_INDEXED_FAMILY = 2,
	PROTOTYPE_READ_DIAGNOSTIC_NESTED_MATCH_GROUPING = 3
};

struct prototype_read_error {
	const char* filename;
	unsigned line;
	unsigned column;
	int diagnostic_code;
	char message[160];
};

int prototype_read_file(
	const char* path,
	struct prototype_program* program,
	struct prototype_read_error* error
);

int prototype_read_string(
	const char* name,
	const char* input,
	struct prototype_program* program,
	struct prototype_read_error* error
);

int prototype_read_ast_file(
	const char* path,
	struct prototype_program* program,
	struct prototype_read_error* error
);

int prototype_read_ast_string(
	const char* name,
	const char* input,
	struct prototype_program* program,
	struct prototype_read_error* error
);

#endif
