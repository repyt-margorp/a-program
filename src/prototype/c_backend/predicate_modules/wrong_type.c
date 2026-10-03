#include "left/component.h"
#include "right/component.h"

/* Deliberate nominal pointer mismatch: clients must copy magnitudes between
	* distinct module Lists instead of passing a differently named node pointer. */
int wrong_type(const struct ap_data_LNumbers *input)
{
	uint32_t value;
	size_t written;
	return ap_copy_RNumbers(input, &value, 1, &written);
}
