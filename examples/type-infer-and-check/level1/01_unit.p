/* Level 1: Finite type - Unit */

Unit := @{
  tt : *;
};

/* Unit eliminator - everything maps to itself */
unit_elim := \A : @ => \a : A => \u : Unit =>
    u @tt => a;
unit_elim :: (A : @) -> A -> Unit -> A;

/* Constant unit function */
const_unit := \A : @ => \a : A => Unit.tt;
const_unit :: (A : @) -> A -> Unit;

main := Unit.tt;
