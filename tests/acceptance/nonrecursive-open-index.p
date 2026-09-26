/* Open post-check: neither recursion nor a concrete Bool may guide synthesis. */
Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:(k:Nat)->A->* k->*(Nat.succ k);
};

choose := \b:Bool => b
	@false => Nat.zero
	@true => Nat.succ Nat.zero;

prepend := \b:Bool => \xs:Vec Nat (choose b) =>
	((Vec Nat).cons (choose b) Nat.zero xs :: Vec Nat (Nat.succ (choose b)));

one := (Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil;
two := (Vec Nat).cons (Nat.succ Nat.zero) Nat.zero one;
main := prepend Bool.false (Vec Nat).nil;
other := prepend Bool.true one;
