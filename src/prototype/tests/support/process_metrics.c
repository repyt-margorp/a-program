#define _GNU_SOURCE

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static uint64_t timespec_microseconds(struct timespec value) {
	return (uint64_t)value.tv_sec * UINT64_C(1000000) +
		(uint64_t)value.tv_nsec / UINT64_C(1000);
}

static uint64_t timeval_microseconds(struct timeval value) {
	return (uint64_t)value.tv_sec * UINT64_C(1000000) +
		(uint64_t)value.tv_usec;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s command [argument ...]\n", argv[0]);
		return 2;
	}
	struct timespec started;
	struct timespec finished;
	if (clock_gettime(CLOCK_MONOTONIC, &started) != 0) {
		perror("clock_gettime");
		return 2;
	}
	pid_t child = fork();
	if (child < 0) {
		perror("fork");
		return 2;
	}
	if (child == 0) {
		execvp(argv[1], &argv[1]);
		perror("execvp");
		_exit(errno == ENOENT ? 127 : 126);
	}
	int status;
	struct rusage usage;
	if (wait4(child, &status, 0, &usage) < 0 ||
		clock_gettime(CLOCK_MONOTONIC, &finished) != 0) {
		perror("wait4/clock_gettime");
		return 2;
	}
	uint64_t wall_us = timespec_microseconds(finished) -
		timespec_microseconds(started);
	fprintf(
		stderr,
		"A_PROGRAM_PROCESS_METRICS 1 wall_us=%llu user_us=%llu system_us=%llu "
		"max_rss_kb=%ld exit=%d signal=%d\n",
		(unsigned long long)wall_us,
		(unsigned long long)timeval_microseconds(usage.ru_utime),
		(unsigned long long)timeval_microseconds(usage.ru_stime),
		usage.ru_maxrss,
		WIFEXITED(status) ? WEXITSTATUS(status) : -1,
		WIFSIGNALED(status) ? WTERMSIG(status) : 0
	);
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 2;
}
