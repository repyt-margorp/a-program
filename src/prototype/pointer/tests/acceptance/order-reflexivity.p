Nat := @{zero:*; succ:*->*;};
LE := @\left:Nat => @\right:Nat => {
	zeroLe : (n:Nat) -> * Nat.zero n;
	succLe : (m:Nat) -> (n:Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};
leRefl := \n:Nat => n
	@zero => LE.zeroLe Nat.zero
	@succ k => LE.succLe k k *k;
leRefl :: (n:Nat) -> LE n n;
one := Nat.succ Nat.zero;
two := Nat.succ one;
main := leRefl two;
expected := LE.succLe one one (LE.succLe Nat.zero Nat.zero (LE.zeroLe Nat.zero));
use := \n:Nat => \proof:LE n n => \graph:@leRefl n proof => proof;
certified := *leRefl two @ proof => use two proof @proof;
depth := \n:Nat => \proof:LE n n => \graph:@leRefl n proof => graph
	@zero => Nat.zero
	@succ k recursive graphIH => Nat.succ *graphIH;
observed := *leRefl two @ proof => depth two proof @proof;
zeroBound := \n:Nat => LE.zeroLe n;
zeroCertified := *zeroBound two @ proof => proof;
zeroExpected := LE.zeroLe two;
