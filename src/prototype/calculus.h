#ifndef __PROTOTYPE_CALCULUS_H__
#define __PROTOTYPE_CALCULUS_H__

/* The current compiler-local calculus identity uses the same 64-hex format as
 * the artifact manifest. It changes whenever the accepted semantic manifest
 * changes; no compatibility alias is retained in the prototype. */
#define PROTOTYPE_CALCULUS_FINGERPRINT \
	"0e625f6dbc282ae473341fd688bf60bfe7c302b9f969c1330b275102883b48cd"

/* This digest identifies the compiler-local parametricity and object Identity
 * vocabulary. Artifact v85 persists only the object roots admitted by both
 * manifests; compiler action/work records remain outside the wire graph. */
#define PROTOTYPE_HOTT_CALCULUS_FINGERPRINT \
	"f0cf064cb17e56e2f842ac0144954c1f31f3a120cee11258de5216d4bef8e781"

#endif
