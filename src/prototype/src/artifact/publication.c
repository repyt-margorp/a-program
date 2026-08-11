#include "a_program/kernel/cwf_certificate.h"

/* Publication remains one translation unit so closure traversal and dense ID
 * assignment retain their exact order. These partitions separate ownership. */
#include "publication/wire_primitives.inc"
#include "publication/closure_marking_and_slices.inc"
#include "publication/section_writers.inc"
#include "publication/dense_publication.inc"
#include "publication/writer.inc"
