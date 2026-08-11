#ifndef __PROTOTYPE_READER_H__
#define __PROTOTYPE_READER_H__

struct prototype_program;

struct prototype_read_error {
	const char* filename;
	unsigned line;
	unsigned column;
	char message[160];
};

struct prototype_read_options {
	int forbid_standalone_expectations;
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

int prototype_read_ast_file_with_options(
	const char* path,
	struct prototype_program* program,
	const struct prototype_read_options* options,
	struct prototype_read_error* error
);

int prototype_read_ast_string(
	const char* name,
	const char* input,
	struct prototype_program* program,
	struct prototype_read_error* error
);

int prototype_read_ast_string_with_options(
	const char* name,
	const char* input,
	struct prototype_program* program,
	const struct prototype_read_options* options,
	struct prototype_read_error* error
);

#endif
