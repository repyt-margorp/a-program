Nat := @{
	zero : *;
	succ : * -> *;
};

NatList := @{
	nil : *;
	cons : Nat -> * -> *;
};

length := \xs : NatList =>
	xs
		@nil => Nat.zero
		@cons head tail => Nat.succ *tail;

length :: NatList -> Nat;

graphOutput := \input : NatList => \output : Nat =>
	\graph : @length input output => output;

one := NatList.cons Nat.zero NatList.nil;

certifiedMain := {
	packet := *length one;
	packet @returned output graph =>
		graphOutput one output graph;
};

main := length one;
expected := {
	Nat.succ Nat.zero;
};
