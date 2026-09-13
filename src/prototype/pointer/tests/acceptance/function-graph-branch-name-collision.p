Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
List := @{nil:*; cons:Bool->Bool->*->*;};
length := \xs:List => xs @nil => Nat.zero
	@cons first second tail => (first
		@false => (second @false => Nat.succ *tail @true => Nat.succ *tail)
		@true => (second @false => Nat.succ *tail @true => Nat.succ *tail));
relation := @length;
