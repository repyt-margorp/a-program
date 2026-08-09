#ifndef __PROTOTYPE_CALCULUS_H__
#define __PROTOTYPE_CALCULUS_H__

/* The current compiler-local calculus identity uses the same 64-hex format as
 * the artifact manifest. It changes whenever the accepted semantic manifest
 * changes; no compatibility alias is retained in the prototype. */
#define PROTOTYPE_CALCULUS_FINGERPRINT \
	"e00224944fff581227696965ec84b2c9dca133e50504e84fd85249602019671e"

/* This digest identifies the compiler-local observational rule vocabulary and
 * ownership contract. It is intentionally independent of the artifact wire
 * manifest until object HOTT records become publication roots. */
#define PROTOTYPE_HOTT_CALCULUS_FINGERPRINT \
	"45d3c0002d5541788ee5b038397aeae718be88237f59d2b88b8117b562c3a0ab"

#endif
