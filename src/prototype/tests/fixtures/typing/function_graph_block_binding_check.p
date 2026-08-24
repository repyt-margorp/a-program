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
		@cons head tail => {
			tailLength := *tail;
			Nat.succ tailLength;
		};

length :: NatList -> Nat;

one := NatList.cons Nat.zero NatList.nil;

main := length one;

expected := {
	Nat.succ Nat.zero;
};

certified := *length one;
