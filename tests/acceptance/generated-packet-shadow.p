Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList => xs @nil=>Nat.zero
	@cons head tail => { tailLength := *tail; Nat.succ tailLength; };
consume := \input:NatList => \n:Nat => \graph:@length input n => n;
valid := \input:NatList => \n:Nat => \graph:@length input n => graph
	@nil => Nat.zero
	@cons { tail; tailLength := output; } => (\ignored:Nat => consume tail output @output) Nat.zero;
bad := \input:NatList => \n:Nat => \graph:@length input n => graph
	@nil => Nat.zero
	@cons { tail; tailLength := output; } => (\output:Nat => consume tail output @output) Nat.zero;
