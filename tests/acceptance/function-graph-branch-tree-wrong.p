Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
List := @{nil:*; cons:Bool->*->*;};
length := \xs:List => xs @nil => Nat.zero
	@cons head tail => (head @false => Nat.succ *tail @true => Nat.succ *tail);
wrong := \xs:List => \n:Nat => \g:@length xs n => g
	@nil => ((@length).nil :: @length xs (Nat.succ Nat.zero))
	@false tail count trace => (@length).false tail count trace
	@true tail count trace => (@length).true tail count trace;
