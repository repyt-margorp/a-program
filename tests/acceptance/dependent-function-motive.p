Nat := @{zero:*; succ:*->*;};
Bool := @{true:*; false:*;};
LE := @\left:Nat => @\right:Nat => {
	zeroLe : (n:Nat) -> * Nat.zero n;
	succLe : (m:Nat) -> (n:Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};
leRefl := \n:Nat =>
	n @zero => (\b:Bool => LE.zeroLe Nat.zero)
	  @succ k => (\b:Bool => LE.succLe k k (*k b));
leRefl :: (n:Nat) -> Bool -> LE n n;
one := Nat.succ Nat.zero;
main := leRefl one Bool.true;
expected := LE.succLe Nat.zero Nat.zero (LE.zeroLe Nat.zero);

dependent := \n:Nat =>
	n @zero => (\m:Nat => \p:LE m m => LE.zeroLe Nat.zero)
	  @succ k => (\m:Nat => \p:LE m m => LE.succLe k k (*k m p));
dependentMain := dependent one Nat.zero (LE.zeroLe Nat.zero);
dependent :: (n:Nat) -> (m:Nat) -> LE m m -> LE n n;
