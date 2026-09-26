Nat := @{zero:*; succ:*->*;};
List := \A:@ => @{nil:*; cons:A->*->*;};
length := \A:@ => \xs:List A => xs
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail;
lengthNat := length Nat;
bad := (@lengthNat).nil;
bad :: @lengthNat ((List Nat).cons Nat.zero (List Nat).nil) Nat.zero;
