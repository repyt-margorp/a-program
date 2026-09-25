Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList => xs
	@nil => Nat.zero
	@cons head tail => { tailLength := *tail; Nat.succ tailLength; };
length_graph := \xs:NatList => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons head tail => (@length).cons head tail (length tail) *tail;
length_graph :: (xs:NatList)->@length xs (length xs);
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
main := inspect two (length two) (length_graph two);
aliasMain := inspectAlias two (length two) (length_graph two);
graphMain := selectGraph two (length two) (length_graph two);
valueMain := selectValue two (length two) (length_graph two);
expected := Nat.succ (Nat.succ Nat.zero);
