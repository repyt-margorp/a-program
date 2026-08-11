/* Level 0: Universe type */

/* A type variable ranges over the universe. */
TypeIdentity := \A : @ => A;
TypeIdentity :: (A : @) -> @;

main := TypeIdentity;
