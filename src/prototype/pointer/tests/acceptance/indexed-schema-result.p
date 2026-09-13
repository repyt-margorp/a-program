Nat := @{ zero : *; succ : * -> *; };
Unit := @{ unit : *; };

Delayed := @\n : Nat => {
	zero : * Nat.zero;
	succ : (k : Nat) -> (Unit -> * k) -> * (Nat.succ k);
};

make := \n : Nat =>
	n @zero => Delayed.zero
	  @succ k => Delayed.succ k &(\u : Unit => *k);
make :: (n : Nat) -> Delayed n;

read := \n : Nat => \value : Delayed n =>
	value @zero => Nat.zero
	      @succ k next => Nat.succ (*next Unit.unit);

two := Nat.succ (Nat.succ Nat.zero);
main := read two (make two);
