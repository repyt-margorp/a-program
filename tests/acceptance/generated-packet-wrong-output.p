Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList => xs @nil=>Nat.zero
	@cons head tail => { tailLength := *tail; Nat.succ tailLength; };
one := NatList.cons Nat.zero NatList.nil;
consume := \input:NatList => \output:Nat => \graph:@length input output => Nat.zero;
valid := \input:NatList => \n:Nat => \graph:@length input n => graph
	@nil => Nat.zero
	@cons { tail; tailLength := output; } => consume tail output @output;
bad := \input:NatList => \n:Nat => \graph:@length input n => graph
	@nil => Nat.zero
	@cons { tail; tailLength := output; } => consume tail (Nat.succ output) @output;
