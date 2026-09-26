Nat := @{zero : *; succ : * -> *;};
Bool := @{false : *; true : *;};
Box := @{box : (A : @) -> A -> *;};

recover := \packet : Box => packet @box A value => value;
choose := \b : Bool => b @true => Nat.zero @false => Bool.true;
nat := recover (Box.box Nat Nat.zero);
bool := recover (Box.box Bool Bool.true);
selectedNat := choose Bool.true;
selectedBool := choose Bool.false;
nat :: Nat;
bool :: Bool;
selectedNat :: Nat;
selectedBool :: Bool;
expectedNat := Nat.zero;
expectedBool := Bool.true;
