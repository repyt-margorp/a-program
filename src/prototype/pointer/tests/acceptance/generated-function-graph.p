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

one := NatList.cons Nat.zero NatList.nil;
main := length one;
certifiedMain := {
	packet := *length one;
	packet @returned output graph => graphOutput one output graph;
};
aliasMain := {
	packet := *alias one;
	packet @returned output graph => aliasOutput one output graph;
};
expected := Nat.succ Nat.zero;
