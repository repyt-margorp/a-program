Nat := @{zero:*; succ:*->*;};
List := @{nil:*; cons:Nat->*->*;};
length := \xs:List => (\ys:List => ys
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail) xs;
Graph := @length;
wrong := Graph.nil;
wrong :: Graph List.nil (Nat.succ Nat.zero);
