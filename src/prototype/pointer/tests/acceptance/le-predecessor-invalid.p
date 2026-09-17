Nat := @{ zero : *; succ : * -> *; };
LE := @\left : Nat => @\right : Nat => {
	zero : (n : Nat) -> * Nat.zero n;
	succ : (m : Nat) -> (n : Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};
// A zero proof cannot stand for an arbitrary pair of predecessor endpoints.
bad := \m : Nat => \n : Nat => \proof : LE (Nat.succ m) (Nat.succ n) => proof
	@succ a b prior => LE.zero b;
bad :: (m : Nat) -> (n : Nat) -> LE (Nat.succ m) (Nat.succ n) -> LE m n;
