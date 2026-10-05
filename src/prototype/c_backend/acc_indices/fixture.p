import Nat;

// Admitted source declarations with explicitly unsupported target index/fields.
two_successors_lt := @\left : Nat => @\right : Nat => {
	step : (n : Nat) -> * n (Nat.succ (Nat.succ n));
	weakenRight : (m : Nat) -> (n : Nat) -> * m n -> * m (Nat.succ n);
	lift : (m : Nat) -> (n : Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};

extra_field_lt := @\left : Nat => @\right : Nat => {
	step : (n : Nat) -> (extra : Nat) -> * n (Nat.succ n);
	weakenRight : (m : Nat) -> (n : Nat) -> * m n -> * m (Nat.succ n);
	lift : (m : Nat) -> (n : Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};

wrong_prior_lt := @\left : Nat => @\right : Nat => {
	step : (n : Nat) -> * n (Nat.succ n);
	weakenRight : (m : Nat) -> (n : Nat) -> (prior : Nat) -> * m (Nat.succ n);
	lift : (m : Nat) -> (n : Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};
