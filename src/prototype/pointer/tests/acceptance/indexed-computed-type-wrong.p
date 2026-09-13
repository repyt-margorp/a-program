Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:(k:Nat)->A->* k->* (Nat.succ k);
};
add := \left:Nat =>
	left @zero => (\right:Nat=>right)
		@succ predecessor => (\right:Nat=>Nat.succ (*predecessor right));
keep := \n:Nat => \xs:Vec Nat (add n n) => xs;
main := keep (Nat.succ Nat.zero) (Vec Nat).nil;
