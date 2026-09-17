Nat := @{ zero : *; succ : * -> *; };
LE := @\left : Nat => @\right : Nat => {
	zero : (n : Nat) -> * Nat.zero n;
	succ : (m : Nat) -> (n : Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};
le_pred := \m : Nat => \n : Nat => \proof : LE (Nat.succ m) (Nat.succ n) => proof
	@succ a b prior => prior;
le_pred :: (m : Nat) -> (n : Nat) -> LE (Nat.succ m) (Nat.succ n) -> LE m n;
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
expected := LE.zero one;
main := le_pred zero one (LE.succ zero one expected);
