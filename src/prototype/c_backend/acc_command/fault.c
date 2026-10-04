/* Test-only faults at the unified driver's private output/stream boundary. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *output;
static int mode(const char *name)
{
	const char *requested=getenv("ACC_COMMAND_IO_FAULT");
	return requested && !strcmp(requested,name);
}

FILE *__real_fopen(const char *, const char *);
FILE *__wrap_fopen(const char *path, const char *access)
{
	int selected=!strcmp(access,"wx") && strstr(path,"/actions.inc");
	if (selected && mode("open")) { errno=EIO; return NULL; }
	FILE *file=__real_fopen(path,access);
	if (selected) output=file;
	return file;
}

size_t __real_fwrite(const void *, size_t, size_t, FILE *);
size_t __wrap_fwrite(const void *data, size_t size, size_t count, FILE *file)
{
	if (file==output && mode("write")) { errno=EIO; return 0; }
	return __real_fwrite(data,size,count,file);
}

int __real_fclose(FILE *);
int __wrap_fclose(FILE *file)
{
	int selected=file==output;
	if (selected) output=NULL;
	int status=__real_fclose(file);
	if (selected && mode("close")) { errno=EIO; return EOF; }
	return status;
}

int __real_mkstemp(char *);
int __wrap_mkstemp(char *path)
{
	if (strstr(path,"/.stream.") && mode("temporary")) { errno=EIO; return -1; }
	return __real_mkstemp(path);
}
