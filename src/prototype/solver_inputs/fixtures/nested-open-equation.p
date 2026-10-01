// Regression: repeated & preserves a Thunk while APP supplies the open Match's carrier.
Nat := @{zero:*; succ:*->*;};
Positive := @\n:Nat=>{step:(k:Nat)->* (Nat.succ k);};
main := (\f:(edge:Positive Nat.zero)->Nat=>Nat.zero)
	&(&(\edge:Positive Nat.zero=>edge @step n=>Missing));
