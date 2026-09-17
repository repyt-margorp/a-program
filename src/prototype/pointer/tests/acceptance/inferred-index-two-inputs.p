Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:A->* n->* (Nat.succ n);
};
Pair := @\n:Nat => @\m:Nat => {mk:Vec Nat n->Vec Nat m->* n m;};
empty := (Vec Nat).nil;
one := Nat.succ Nat.zero;
singleton := (Vec Nat).cons Nat.zero empty;
partial := Pair.mk singleton;
main := partial empty;
main :: Pair one Nat.zero;
expected := Pair.mk singleton empty;
