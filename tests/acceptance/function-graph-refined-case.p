Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList => xs
	@nil => Nat.zero
	@cons head tail => { tailLength := *tail; Nat.succ tailLength; };
length_graph := \xs:NatList => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons head tail => (@length).cons head tail (length tail) *tail;
length_graph :: (xs:NatList)->@length xs (length xs);
inspect := \head:Nat => \tail:NatList => \output:Nat =>
	\graph:@length (NatList.cons head tail) output => graph
	@cons { tailLength; } => Nat.succ tailLength;
one := NatList.cons Nat.zero NatList.nil;
input := NatList.cons Nat.zero one;
main := inspect Nat.zero one (length input) (length_graph input);
expected := Nat.succ (Nat.succ Nat.zero);
