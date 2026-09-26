Nat := @{zero:*; succ:*->*;};
Box := @\n:Nat => {mk:(n:Nat)->* n;};
consume := \n:Nat => n
	@zero => (\p:Box Nat.zero => Nat.zero)
	@succ k => (\p:Box (Nat.succ k) => Nat.zero);
main := consume (Nat.succ Nat.zero) (Box.mk Nat.zero);
