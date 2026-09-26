#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <time.h>

int main(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
		now.tv_nsec < 0) {
		return 1;
	}
	uint64_t milliseconds = (uint64_t)now.tv_sec * UINT64_C(1000) +
		(uint64_t)now.tv_nsec / UINT64_C(1000000);
	if (printf("%llu\n", (unsigned long long)milliseconds) < 0) {
		return 1;
	}
	return 0;
}
