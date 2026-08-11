/* Level 1: Finite type - Bool */

Bool := @{
  true : *;
  false : *;
};

/* Boolean negation */
not := \b : Bool =>
  b @true => Bool.false
    @false => Bool.true;
not :: Bool -> Bool;

/* Boolean and */
and := \a : Bool => \b : Bool =>
  a @true => b
    @false => Bool.false;
and :: Bool -> Bool -> Bool;

/* Boolean or */
or := \a : Bool => \b : Bool =>
  a @true => Bool.true
    @false => b;
or :: Bool -> Bool -> Bool;

main := not Bool.true;
