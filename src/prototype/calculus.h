#ifndef __PROTOTYPE_CALCULUS_H__
#define __PROTOTYPE_CALCULUS_H__

/* The current compiler-local calculus identity uses the same 64-hex format as
 * the artifact manifest. It changes whenever the accepted semantic manifest
 * changes; no compatibility alias is retained in the prototype. */
#define PROTOTYPE_CALCULUS_FINGERPRINT \
	"cdaff4eb6415fa797fbc872b11dd257dc19d866d680aef27e6cd8a4f5df0f962"

/* This digest identifies the compiler-local parametricity and object Identity
 * vocabulary. Artifact v73 persists only the object roots admitted by both
 * manifests; compiler action/work records remain outside the wire graph. */
#define PROTOTYPE_HOTT_CALCULUS_FINGERPRINT \
	"3ef5f2c9b2d9b4d68e61b8476f51f4d010e34b0f654cd45c469c47c2fa7d9eb6"

#endif
