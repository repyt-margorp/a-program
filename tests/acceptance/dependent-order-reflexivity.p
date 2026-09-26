Nat := @{zero:*; succ:*->*;};
LE := @\left:Nat => @\right:Nat => {
	zeroLe : (n:Nat) -> * Nat.zero n;
	succLe : (m:Nat) -> (n:Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};
leRefl := \n:Nat =>
	n @zero => LE.zeroLe Nat.zero
	  @succ k => LE.succLe k k *k;
leRefl :: (n:Nat) -> LE n n;
one := Nat.succ Nat.zero;
main := leRefl one;
expected := LE.succLe Nat.zero Nat.zero (LE.zeroLe Nat.zero);
