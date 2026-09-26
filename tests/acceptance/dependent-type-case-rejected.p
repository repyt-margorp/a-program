Nat := @{zero : *; succ : * -> *;};
Bool := @{false : *; true : *;};
Box := @{box : (A : @) -> A -> *;};
recover := \packet : Box => packet @box A value => value;
main := recover (Box.box Bool Bool.true);
main :: Nat;
