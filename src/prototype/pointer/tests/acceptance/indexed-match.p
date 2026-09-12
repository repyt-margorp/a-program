Nat := @{ zero : *; succ : * -> *; };
Indexed := @\n : Nat => {
	at : (k : Nat) -> * k;
	next : (k : Nat) -> * k -> * (Nat.succ k);
};
getIndex := \n : Nat => \v : Indexed n =>
	v @at k => k
	  @next k previous => Nat.succ k;
countSteps := \n : Nat => \v : Indexed n =>
	v @at k => Nat.zero
	  @next k previous => Nat.succ *previous;
one := Indexed.next Nat.zero (Indexed.at Nat.zero);
main := getIndex (Nat.succ Nat.zero) one;
expected := Nat.succ Nat.zero;
recursive := countSteps (Nat.succ Nat.zero) one;
recursive :: Nat;
twoSteps := countSteps (Nat.succ (Nat.succ Nat.zero)) (Indexed.next (Nat.succ Nat.zero) one);
twoExpected := Nat.succ expected;
