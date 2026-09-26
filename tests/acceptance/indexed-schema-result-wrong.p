Nat := @{ zero : *; succ : * -> *; };
Unit := @{ unit : *; };

Delayed := @\n : Nat => {
	zero : * Nat.zero;
	succ : (k : Nat) -> (Unit -> * k) -> * (Nat.succ k);
};

make := \n : Nat =>
	n @zero => Delayed.zero
	  @succ k => Delayed.succ (Nat.succ k) &(\u : Unit => *k);
