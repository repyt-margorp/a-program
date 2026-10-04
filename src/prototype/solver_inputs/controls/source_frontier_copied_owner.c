/* Run each permanent boundary independently so the first parent assertion
 * cannot hide the other capture/prepare/attachment verdicts. */
#define main source_checkpoint_main
#include "source_checkpoint_test.c"
#undef main

int main(int argc, char **argv)
{
	if (argc == 1) return source_checkpoint_main();
	assert(argc == 3 && strlen(argv[1]) == 1 && strlen(argv[2]) == 1);
	assert(argv[1][0] >= '0' && argv[1][0] <= '2');
	assert(argv[2][0] == '0' || argv[2][0] == '1');
	source_copied_owner((unsigned)(argv[1][0] - '0'), argv[2][0] - '0');
	return 0;
}
