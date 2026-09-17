Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:A->* n->* (Nat.succ n);
};
Pair := @\n:Nat => {mk:Vec Nat n->Vec Nat n->* n;};
empty := (Vec Nat).nil;
one := (Vec Nat).cons Nat.zero empty;
main := Pair.mk empty one;
