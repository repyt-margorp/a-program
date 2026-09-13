Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList => xs
	@nil => Nat.zero
	@cons head tail => { tailLength := *tail; Nat.succ tailLength; };
inspect := \head:Nat => \tail:NatList => \output:Nat =>
	\graph:@length (NatList.cons head tail) output => graph
	@cons { tailLength; } => Nat.succ tailLength;
one := NatList.cons Nat.zero NatList.nil;
input := NatList.cons Nat.zero one;
main := *length input @output => inspect Nat.zero one output @output;
expected := Nat.succ (Nat.succ Nat.zero);
