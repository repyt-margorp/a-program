Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList => xs
	@nil => Nat.zero
	@cons head tail => { tailLength := Nat.succ *tail; tailLength; };
bad := \input:NatList => \output:Nat => \graph:@length input output => graph
	@nil => Nat.zero
	@cons { tailLength; } => tailLength;
