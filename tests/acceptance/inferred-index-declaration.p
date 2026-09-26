Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:A->* n->* (Nat.succ n);
};
length := \A:@ => \n:Nat => \xs:Vec A n => xs
	@nil => Nat.zero
	@cons head tail => Nat.succ (*tail);
one := Nat.succ Nat.zero;
two := Nat.succ one;
singleton := (Vec Nat).cons Nat.zero (Vec Nat).nil;
prepend := (Vec Nat).cons Nat.zero;
quoted := &prepend;
constructor := &(Vec Nat).cons;
open := \n:Nat => \xs:Vec Nat n => constructor Nat.zero xs;
apply := \f:(n:Nat)->Nat->Vec Nat n->Vec Nat (Nat.succ n) =>
	f Nat.zero Nat.zero (Vec Nat).nil;
through := apply &(Vec Nat).cons;
pair := quoted through;
main := length Nat two pair;
expected := two;
