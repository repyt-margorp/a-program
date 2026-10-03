#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Exercise visibility-map stream and close errors through real FILE state. */
static FILE *faulted;

FILE *fopen(const char *path, const char *mode)
{
	FILE *(*open_file)(const char *, const char *) = dlsym(RTLD_NEXT, "fopen");
	if (!open_file) _exit(120);
	const char *fault = getenv("C_SHARED_FAULT"), *tail = strrchr(path, '/');
	if (!fault || strcmp(mode, "w") || !tail || strcmp(tail, "/symbols.map")) return open_file(path, mode);
	FILE *file = open_file("/dev/full", "w");
	if (!file) _exit(121);
	static char buffer[1048576];
	int buffered = !strcmp(fault, "close");
	if (setvbuf(file, buffered ? buffer : NULL, buffered ? _IOFBF : _IONBF,
		buffered ? sizeof(buffer) : 0)) _exit(122);
	faulted = file;
	return file;
}

int fclose(FILE *file)
{
	int affected = file == faulted, failed = affected ? ferror(file) : 0;
	int (*close_file)(FILE *) = dlsym(RTLD_NEXT, "fclose");
	if (!close_file) _exit(123);
	int closed = close_file(file);
	if (affected) {
		faulted = NULL;
		fprintf(stderr, "shared map test: ferror=%d fclose=%d\n", failed, closed);
	}
	return closed;
}
