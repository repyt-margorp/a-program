Nat := @{zero : *; succ : * -> *;};
NatList := @{nil : *; cons : Nat -> * -> *;};
length := \xs : NatList =>
	xs @nil => Nat.zero @cons head tail => Nat.succ *tail;
other := \xs : NatList =>
	xs @nil => Nat.zero @cons head tail => Nat.succ *tail;
consume := \input : NatList => \output : Nat =>
	\graph : @length input output => graph;
bad := \input : NatList => \output : Nat =>
	\graph : @other input output => consume input output graph;
