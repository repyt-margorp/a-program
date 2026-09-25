Nat := @{zero:*; succ:*->*;};
LE := @\left:Nat => @\right:Nat => {
	zeroLe : (n:Nat) -> * Nat.zero n;
	succLe : (m:Nat) -> (n:Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};
leRefl := \n:Nat => n
	@zero => LE.zeroLe Nat.zero
	@succ k => LE.succLe k k *k;
leRefl :: (n:Nat) -> LE n n;
refl_graph := \n:Nat => n @(self => @leRefl self (leRefl self))
	@zero => (@leRefl).zero
	@succ k => (@leRefl).succ k (leRefl k) *k;
refl_graph :: (n:Nat)->@leRefl n (leRefl n);
one := Nat.succ Nat.zero;
two := Nat.succ one;
main := leRefl two;
expected := LE.succLe one one (LE.succLe Nat.zero Nat.zero (LE.zeroLe Nat.zero));
use := \n:Nat => \proof:LE n n => \graph:@leRefl n proof => proof;
certified := use two (leRefl two) (refl_graph two);
depth := \n:Nat => \proof:LE n n => \graph:@leRefl n proof => graph
	@zero => Nat.zero
	@succ k recursive graphIH => Nat.succ *graphIH;
observed := depth two (leRefl two) (refl_graph two);
zeroBound := \n:Nat => LE.zeroLe n;
zeroGraph := @zeroBound;
zeroCertified := zeroBound two;
zeroExpected := LE.zeroLe two;
