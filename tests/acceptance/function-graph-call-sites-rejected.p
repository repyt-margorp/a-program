Nat := @{zero:*; succ:*->*;};
twice := \n:Nat => n
	@zero => Nat.zero
	@succ k => { first:=*k; second:=*k; Nat.succ second; };
base := (@twice).zero;
main := (@twice).succ Nat.zero Nat.zero base (Nat.succ Nat.zero) base;
