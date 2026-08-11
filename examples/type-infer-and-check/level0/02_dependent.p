/* Level 0: Dependent function types (Π-types) */

/* Dependent identity */
dep_id := \A : @ => \x : A => x;
dep_id :: (A : @) -> A -> A;

/* Dependent constant */
dep_const := \A : @ => \B : @ => \a : A => \b : B => a;
dep_const :: (A : @) -> (B : @) -> A -> B -> A;

/* Type-level application */
app_type := \F : ((A : @) -> @) => \A : @ => F A;
app_type :: (F : ((A : @) -> @)) -> (A : @) -> @;

main := dep_id;
