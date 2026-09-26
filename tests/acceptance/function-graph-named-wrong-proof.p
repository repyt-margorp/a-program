Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList => xs
	@nil => Nat.zero
	@cons head tail => { tailLength := *tail; Nat.succ tailLength; };
consume := \input:NatList => \output:Nat => \graph:@length input output => output;
bad := \input:NatList => \output:Nat => \graph:@length input output => graph
	@nil => Nat.zero
	@cons { tail; tailLength := recursive; } => consume tail (Nat.succ recursive) @recursive;
