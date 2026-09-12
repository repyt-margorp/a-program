Nat := @{zero : *; succ : * -> *;};
first := \left : Nat => \right : Nat => left;
consume := \left : Nat => \right : Nat => \output : Nat =>
	\graph : @first left right output => output;
zero := Nat.zero;
one := Nat.succ Nat.zero;
bad := {
	packet := *first zero one;
	packet @returned output graph => consume zero one one graph;
};
