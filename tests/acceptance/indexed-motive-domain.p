// A family index occurs in the returned function's domain, not its result.
Nat := @{zero:*; succ:*->*;};
Box := @\n:Nat => {mk:(n:Nat)->* n;};
consume := \n:Nat => n
	@zero => (\p:Box Nat.zero => Nat.zero)
	@succ k => (\p:Box (Nat.succ k) => Nat.zero);
consume :: (n:Nat)->Box n->Nat;
main := consume (Nat.succ Nat.zero) (Box.mk (Nat.succ Nat.zero));
expected := Nat.zero;
