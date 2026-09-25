Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList => xs
	@nil => Nat.zero
	@cons head tail => { tailLength := *tail; Nat.succ tailLength; };
inspect := \input:NatList => \output:Nat => \graph:@length input output => graph
	@cons { tailLength; } => Nat.succ tailLength;
main := inspect NatList.nil Nat.zero (@length).nil;
expected := { Nat.zero; };
