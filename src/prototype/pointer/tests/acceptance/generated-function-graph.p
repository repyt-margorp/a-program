Nat := @{zero : *; succ : * -> *;};
NatList := @{nil : *; cons : Nat -> * -> *;};

length := \xs : NatList =>
	xs @nil => Nat.zero
	   @cons head tail => Nat.succ *tail;

graphOutput := \input : NatList => \output : Nat =>
	\graph : @length input output => output;

alias := length;
aliasOutput := \input : NatList => \output : Nat =>
	\graph : @alias input output => graphOutput input output graph;

main := length (NatList.cons Nat.zero NatList.nil);
expected := Nat.succ Nat.zero;
