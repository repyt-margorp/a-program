Nat := @{zero : *; succ : * -> *;};
first := \left : Nat => \right : Nat => left;
first_graph := \left:Nat => \right:Nat => (@first left).case0 right;
first_graph :: (left:Nat)->(right:Nat)->@first left right (first left right);
consume := \left : Nat => \right : Nat => \output : Nat =>
	\graph : @first left right output => output;
expected := Nat.zero;
other := Nat.succ Nat.zero;
main := first expected other;
certified := consume expected other (first expected other) (first_graph expected other);
