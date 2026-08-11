/* TermDB remains one translation unit so static helper visibility and term
 * allocation order stay unchanged. The include partitions expose ownership
 * without introducing a second implementation of any term rule. */
#include "term/declarations.inc"
#include "term/canonicalization.inc"
#include "term/storage_and_formation.inc"
#include "term/substitution.inc"
#include "term/evaluation_and_conversion.inc"
