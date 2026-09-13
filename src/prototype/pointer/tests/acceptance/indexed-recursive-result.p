Nat := @{ zero : *; succ : * -> *; };
Unit := @{ unit : *; };

Delayed := @\n : Nat => {
	zero : * Nat.zero;
	succ : (n : Nat) -> (k : Nat) -> (Unit -> * k) -> * n;
};

make := \n : Nat =>
	n @zero => Delayed.zero
	  @succ k => Delayed.succ (Nat.succ k) k &(\u : Unit => *k);
make :: (n : Nat) -> Delayed n;

read := \n : Nat => \value : Delayed n =>
	value @zero => Nat.zero
	      @succ n k next => Nat.succ (*next Unit.unit);

two := Nat.succ (Nat.succ Nat.zero);
main := read two (make two);
emptyMain := read Nat.zero (make Nat.zero);
emptyExpected := Nat.zero;
