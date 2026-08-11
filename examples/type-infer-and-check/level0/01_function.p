/* Level 0: Simple function types */

/* Identity function */
id := \A : @ => A;
id :: (A : @) -> @;

/* Constant function */
const := \A : @ => \B : @ => A;
const :: (A : @) -> (B : @) -> @;

/* Function composition type */
compose_type := \F : ((A : @) -> @) => \G : ((A : @) -> @) =>
  \A : @ => F (G A);
compose_type :: (F : ((A : @) -> @)) -> (G : ((A : @) -> @)) ->
  (A : @) -> @;

main := id;
