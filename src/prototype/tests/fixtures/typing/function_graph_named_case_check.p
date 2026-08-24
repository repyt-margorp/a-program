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

inspect := \input : NatList => \output : Nat =>
	\graph : @length input output =>
	graph
		@nil => Nat.zero
		@cons {
			tailLength;
		} => Nat.succ *tailLength;

selectGraph := \input : NatList => \output : Nat =>
	\graph : @length input output =>
	graph
		@cons {
			tailLength := recursive;
		} => @recursive;

package := *length one;

directValue := *length one @ output => output;

proof := *length one @ output => inspect one output @output;
