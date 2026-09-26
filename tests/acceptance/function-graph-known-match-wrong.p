Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
List := @{nil:*; cons:Nat->*->*;};
length := \xs:List => (\ys:List => ys
	@nil => Nat.zero
	@cons head tail => (Bool.true @false => Nat.zero @true => Nat.succ *tail)) xs;
Graph := @length;
wrong := Graph.cons Nat.zero List.nil Nat.zero Graph.nil;
wrong :: Graph (List.cons Nat.zero List.nil) Nat.zero;
