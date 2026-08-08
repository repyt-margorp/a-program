#ifndef __PROTOTYPE_CALCULUS_H__
#define __PROTOTYPE_CALCULUS_H__

/* The current compiler-local calculus identity uses the same 64-hex format as
 * the artifact manifest. It changes whenever the accepted semantic manifest
 * changes; no compatibility alias is retained in the prototype. */
#define PROTOTYPE_CALCULUS_FINGERPRINT \
	"f5f32952ee941bcb393d3f9f09f789d921edbc1b349d116f6cb34bd1c3203ceb"

/* This digest identifies the compiler-local observational rule vocabulary and
 * ownership contract. It is intentionally independent of the artifact wire
 * manifest until object HOTT records become publication roots. */
#define PROTOTYPE_HOTT_CALCULUS_FINGERPRINT \
	"bc3a8eba64fc16ace410ed022cabc62fef91d29c2fd85d9367eccf1e378cc4c2"

#endif
