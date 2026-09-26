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

bad := \input : NatList => \output : Nat =>
	\graph : @length input output =>
	graph
		@cons {
			missing;
		} => missing;
