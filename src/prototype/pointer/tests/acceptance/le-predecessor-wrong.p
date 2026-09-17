Nat := @{ zero : *; succ : * -> *; };
LE := @\left : Nat => @\right : Nat => {
	zero : (n : Nat) -> * Nat.zero n;
	succ : (m : Nat) -> (n : Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};
// The predecessor proof cannot be reversed by index refinement.
le_pred := \m : Nat => \n : Nat => \proof : LE (Nat.succ m) (Nat.succ n) => proof
	@succ a b prior => prior;
le_pred :: (m : Nat) -> (n : Nat) -> LE (Nat.succ m) (Nat.succ n) -> LE n m;
