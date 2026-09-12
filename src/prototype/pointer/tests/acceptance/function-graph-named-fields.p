Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList => xs
	@nil => Nat.zero
	@cons head tail => { tailLength := *tail; Nat.succ tailLength; };
one := NatList.cons Nat.zero NatList.nil;
two := NatList.cons Nat.zero one;
inspect := \input:NatList => \output:Nat => \graph:@length input output => graph
	@nil => Nat.zero
	@cons { tailLength; } => Nat.succ *tailLength;
inspectAlias := \input:NatList => \output:Nat => \graph:@length input output => graph
	@cons { tailLength := recursive; } => Nat.succ *recursive
	@nil => Nat.zero;
consume := \input:NatList => \output:Nat => \graph:@length input output => output;
selectGraph := \input:NatList => \output:Nat => \graph:@length input output => graph
	@nil => Nat.zero
	@cons { tail; tailLength := recursive; } => Nat.succ (consume tail recursive @recursive);
selectValue := \input:NatList => \output:Nat => \graph:@length input output => graph
	@nil => Nat.zero
	@cons { tailLength := n; } => Nat.succ n;
main := *length two @ output => inspect two output @output;
aliasMain := *length two @ output => inspectAlias two output @output;
graphMain := *length two @ output => selectGraph two output @output;
valueMain := *length two @ output => selectValue two output @output;
expected := Nat.succ (Nat.succ Nat.zero);
