Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:A->* n->* (Nat.succ n);
};
D := @\n:Nat => {mk:Vec Nat (Nat.succ n)->Vec Nat n->* n;};
nil := (Vec Nat).nil;
one := (Vec Nat).cons Nat.zero nil;
main := D.mk one nil;
main :: D Nat.zero;
explicit := \c:(n:Nat)->Vec Nat (Nat.succ n)->Vec Nat n->D n => c Nat.zero one nil;
expected := explicit &D.mk;
