Nat := @{zero:*; succ:*->*;};
List := @{nil:*; cons:Nat->*->*;};
length := \xs:List => xs
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail;
adequacy := \xs:List => *length xs @output => @output;
adequacy :: (xs:List) -> @length xs (length xs);
