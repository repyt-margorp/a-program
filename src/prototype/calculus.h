#ifndef __PROTOTYPE_CALCULUS_H__
#define __PROTOTYPE_CALCULUS_H__

/* The current compiler-local calculus identity uses the same 64-hex format as
 * the artifact manifest. It changes whenever the accepted semantic manifest
 * changes; no compatibility alias is retained in the prototype. */
#define PROTOTYPE_CALCULUS_FINGERPRINT \
	"a0bb662b81ab9d7cef020d53f6c2ac65a9d12ad8277a6a083b2c77e8faa457c4"

/* This digest identifies the compiler-local parametricity and object Identity
 * vocabulary. Artifact v74 persists only the object roots admitted by both
 * manifests; compiler action/work records remain outside the wire graph. */
#define PROTOTYPE_HOTT_CALCULUS_FINGERPRINT \
	"3ef5f2c9b2d9b4d68e61b8476f51f4d010e34b0f654cd45c469c47c2fa7d9eb6"

#endif
