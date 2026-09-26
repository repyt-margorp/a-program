Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:(k:Nat)->A->* k->*(Nat.succ k);
};
add := \m:Nat => m
	@zero => (\n:Nat => n)
	@succ k => (\n:Nat => Nat.succ (*k n));

bridge := \m:Nat => \n:Nat => \xs:Vec Nat (add (Nat.succ m) n) =>
	(xs :: Vec Nat (Nat.succ (add m n)));
input := (Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil;
main := bridge Nat.zero Nat.zero input;
expected := input;
