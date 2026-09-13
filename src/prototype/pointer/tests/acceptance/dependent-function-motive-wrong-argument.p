Nat := @{zero:*; succ:*->*;};
Bool := @{true:*; false:*;};
LE := @\left:Nat => @\right:Nat => {
	zeroLe : (n:Nat) -> * Nat.zero n;
	succLe : (m:Nat) -> (n:Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};
leRefl := \n:Nat =>
	n @zero => (\b:Bool => LE.zeroLe Nat.zero)
	  @succ k => (\b:Bool => LE.succLe k k (*k Nat.zero));
