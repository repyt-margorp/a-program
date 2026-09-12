Nat := @{zero : *; succ : * -> *;};
first := \left : Nat => \right : Nat => left;
consume := \left : Nat => \right : Nat => \output : Nat =>
	\graph : @first left right output => output;
expected := Nat.zero;
other := Nat.succ Nat.zero;
main := first expected other;
certified := {
	packet := *first expected other;
	packet @returned output graph => consume expected other output graph;
};
