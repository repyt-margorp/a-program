Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:(k:Nat)->A->* k->* (Nat.succ k);
};
add := \left:Nat =>
	left @zero => (\right:Nat=>right)
		@succ predecessor => (\right:Nat=>Nat.succ (*predecessor right));
keep := \n:Nat => \xs:Vec Nat (add n n) => xs;
one := Nat.succ Nat.zero;
input := (Vec Nat).cons one Nat.zero ((Vec Nat).cons Nat.zero one (Vec Nat).nil);
main := keep one input;
expected := input;
