(* Level 1: Finite type - Unit *)

Unit := @{
  tt : *;
};

(* Unit eliminator - everything maps to itself *)
unit_elim : (A : *) -> A -> Unit -> A :=
  \A : * => \a : A => \u : Unit =>
    u @tt => a;

(* Constant unit function *)
const_unit : (A : *) -> A -> Unit :=
  \A : * => \a : A => Unit.tt;

main := Unit.tt;
