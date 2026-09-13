Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:(k:Nat)->A->* k->*(Nat.succ k);
};

choose := \b:Bool => b
	@false => Nat.zero
	@true => Nat.succ Nat.zero;

/* Inference succeeds; the open post-check is a separate known limitation. */
prepend := \b:Bool => \xs:Vec Nat (choose b) =>
	(Vec Nat).cons (choose b) Nat.zero xs;

one := (Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil;
two := (Vec Nat).cons (Nat.succ Nat.zero) Nat.zero one;
main := (prepend Bool.false (Vec Nat).nil :: Vec Nat (Nat.succ (choose Bool.false)));
other := (prepend Bool.true one :: Vec Nat (Nat.succ (choose Bool.true)));
