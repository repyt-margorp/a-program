#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Test-only interposition: force a real FILE error without changing admission. */
static FILE *faulted;

static FILE *fault_stream(int buffered)
{
	FILE *(*open_file)(const char *, const char *) = dlsym(RTLD_NEXT, "fopen");
	if (!open_file) _exit(120);
	FILE *file = open_file("/dev/full", "w");
	if (!file) _exit(121);
	static char buffer[1048576];
	if (setvbuf(file, buffered ? buffer : NULL, buffered ? _IOFBF : _IONBF,
		buffered ? sizeof(buffer) : 0)) _exit(122);
	faulted = file;
	fprintf(stderr, "publication test: %s /dev/full stream\n", buffered ? "buffered" : "unbuffered");
	return file;
}

static int suffix(const char *path, const char *tail)
{
	size_t length = strlen(path), tail_length = strlen(tail);
	return length >= tail_length && !strcmp(path + length - tail_length, tail);
}

FILE *fopen(const char *path, const char *mode)
{
	const char *fault = getenv("C_PUBLICATION_FAULT");
	if (fault && !strcmp(mode, "w") &&
		(((!strcmp(fault, "source") || !strcmp(fault, "source-close")) && suffix(path, "/component.c")) ||
		 ((!strcmp(fault, "header") || !strcmp(fault, "header-close")) && suffix(path, "/component.h")) ||
		 (!strcmp(fault, "receipt") && suffix(path, "/link.json"))))
		return fault_stream(suffix(fault, "-close"));
	FILE *(*open_file)(const char *, const char *) = dlsym(RTLD_NEXT, "fopen");
	if (!open_file) _exit(120);
	return open_file(path, mode);
}

FILE *fdopen(int fd, const char *mode)
{
	const char *fault = getenv("C_PUBLICATION_FAULT");
	if (fault && (!strcmp(fault, "direct") || !strcmp(fault, "direct-close")) && !strcmp(mode, "w")) {
		FILE *file = fault_stream(suffix(fault, "-close"));
		close(fd);
		return file;
	}
	FILE *(*open_fd)(int, const char *) = dlsym(RTLD_NEXT, "fdopen");
	if (!open_fd) _exit(123);
	return open_fd(fd, mode);
}

int fclose(FILE *file)
{
	int affected = file == faulted, failed = affected ? ferror(file) : 0;
	int (*close_file)(FILE *) = dlsym(RTLD_NEXT, "fclose");
	if (!close_file) _exit(124);
	int closed = close_file(file);
	if (affected) {
		faulted = NULL;
		fprintf(stderr, "publication test: ferror=%d fclose=%d\n", failed, closed);
	}
	return closed;
}
