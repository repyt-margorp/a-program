/* Level 0: Dependent function types (Π-types) */

/* Dependent identity */
dep_id : (A : *) -> A -> A := \A : * => \x : A => x;

/* Dependent constant */
dep_const : (A : *) -> (B : *) -> A -> B -> A :=
  \A : * => \B : * => \a : A => \b : B => a;

/* Type-level application */
app_type : (F : * -> *) -> * -> * :=
  \F : (* -> *) => \A : * => F A;

main := dep_id;
