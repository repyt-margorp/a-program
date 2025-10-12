(* Level 1: Finite type - Bool *)

Bool := @{
  true : *;
  false : *;
};

(* Boolean negation *)
not : Bool -> Bool := \b : Bool =>
  b @true => Bool.false
    @false => Bool.true;

(* Boolean and *)
and : Bool -> Bool -> Bool := \a : Bool => \b : Bool =>
  a @true => b
    @false => Bool.false;

(* Boolean or *)
or : Bool -> Bool -> Bool := \a : Bool => \b : Bool =>
  a @true => Bool.true
    @false => b;

main := not Bool.true;
