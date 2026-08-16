#ifndef __PROTOTYPE_CALCULUS_H__
#define __PROTOTYPE_CALCULUS_H__

/* The current compiler-local calculus identity uses the same 64-hex format as
 * the artifact manifest. It changes whenever the accepted semantic manifest
 * changes; no compatibility alias is retained in the prototype. */
#define PROTOTYPE_CALCULUS_FINGERPRINT \
	"dcff8f369e62cd5841e5ba5e4d733c18cb0942b355eb22313d2a61a9fd326c46"

/* This digest identifies the compiler-local parametricity and object Identity
 * vocabulary. Artifact v75 persists only the object roots admitted by both
 * manifests; compiler action/work records remain outside the wire graph. */
#define PROTOTYPE_HOTT_CALCULUS_FINGERPRINT \
	"3ef5f2c9b2d9b4d68e61b8476f51f4d010e34b0f654cd45c469c47c2fa7d9eb6"

#endif
