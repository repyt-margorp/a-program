Nat := @{ zero : *; succ : * -> *; };
Indexed := @\n : Nat => {
	at : (k : Nat) -> * k;
	next : (k : Nat) -> * k -> * (Nat.succ k);
};
ZeroFiber := Indexed Nat.zero;
zero := Indexed.at Nat.zero;
main := Indexed.next Nat.zero zero;
