Nat := @{ zero : *; succ : * -> *; };
Unit := @{ unit : *; };

Delayed := @\n : Nat => {
	zero : * Nat.zero;
	succ : (n : Nat) -> (k : Nat) -> (Unit -> * k) -> * n;
};

make := \n : Nat =>
	n @zero => Delayed.zero
	  @succ k => Delayed.succ (Nat.succ k) (Nat.succ k) &(\u : Unit => *k);
