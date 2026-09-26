Nat := @{zero : *; succ : * -> *;};
NatList := @{nil : *; cons : Nat -> * -> *;};
length := \xs : NatList =>
	xs @nil => Nat.zero @cons head tail => Nat.succ *tail;
graphOutput := \input : NatList => \output : Nat =>
	\graph : @length input output => output;
one := NatList.cons Nat.zero NatList.nil;
graph := (@length).cons Nat.zero NatList.nil Nat.zero (@length).nil;
valid := graphOutput one (length one) graph;
bad := graphOutput one Nat.zero graph;
