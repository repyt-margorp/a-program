#ifndef __PROTOTYPE_CALCULUS_H__
#define __PROTOTYPE_CALCULUS_H__

/* The current compiler-local calculus identity uses the same 64-hex format as
 * the artifact manifest. It changes whenever the accepted semantic manifest
 * changes; no compatibility alias is retained in the prototype. */
#define PROTOTYPE_CALCULUS_FINGERPRINT \
	"9cd24fd4d50871b5a3622f757cf1343f557cc6422a28eca87470d886119e273c"

/* This digest identifies the compiler-local relational rule vocabulary and
 * ownership contract. It is intentionally independent of the artifact wire
 * manifest until object HOTT records become publication roots. */
#define PROTOTYPE_HOTT_CALCULUS_FINGERPRINT \
	"a3616764e6faa7d723708139e79a81794d54342bcc9477bf44c071455e62030b"

#endif
