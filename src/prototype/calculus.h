#ifndef __PROTOTYPE_CALCULUS_H__
#define __PROTOTYPE_CALCULUS_H__

/* The current compiler-local calculus identity uses the same 64-hex format as
 * the artifact manifest. It changes whenever the accepted semantic manifest
 * changes; no compatibility alias is retained in the prototype. */
#define PROTOTYPE_CALCULUS_FINGERPRINT \
	"31fc96a8ce13d4771250ac6ad2c2814f4ee4e7d1f04563a6e4165d825f86c2ef"

/* This digest identifies the compiler-local parametricity and object Identity
 * vocabulary. Artifact v78 persists only the object roots admitted by both
 * manifests; compiler action/work records remain outside the wire graph. */
#define PROTOTYPE_HOTT_CALCULUS_FINGERPRINT \
	"f0cf064cb17e56e2f842ac0144954c1f31f3a120cee11258de5216d4bef8e781"

#endif
