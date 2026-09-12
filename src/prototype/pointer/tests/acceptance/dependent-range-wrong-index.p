Nat := @{zero:*; succ:*->*;};
LE := @\left:Nat => @\right:Nat => {
	zeroLe : (n:Nat) -> * Nat.zero n;
	succLe : (m:Nat) -> (n:Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};
leRefl := \n:Nat => n @zero=>LE.zeroLe Nat.zero @succ k=>LE.succLe k k *k;
bad := \n:Nat => \graph:@leRefl n (LE.zeroLe Nat.zero) => Nat.zero;
