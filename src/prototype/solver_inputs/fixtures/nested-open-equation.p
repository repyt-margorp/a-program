// Existing pending-structure cycle; not a passing acceptance fixture.
Nat := @{zero:*; succ:*->*;};
Positive := @\n:Nat=>{step:(k:Nat)->* (Nat.succ k);};
main := (\f:(edge:Positive Nat.zero)->Nat=>Nat.zero)
	&(&(\edge:Positive Nat.zero=>edge @step n=>Missing));
