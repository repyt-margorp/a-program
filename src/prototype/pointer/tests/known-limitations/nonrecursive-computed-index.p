/* A finite control for computed-index substitution; not a passing test. */
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
