Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:(k:Nat)->A->* k->*(Nat.succ k);
};
choose := \b:Bool => b
	@false => Nat.zero
	@true => Nat.succ Nat.zero;

/* Result projection cannot erase the constructor's successor index. */
wrong := \b:Bool => \xs:Vec Nat (choose b) =>
	((Vec Nat).cons (choose b) Nat.zero xs :: Vec Nat (choose b));
