// Retained reductions may rename binders inside an index's declared type.
Nat := @{zero:*;succ:*->*;};
pred := \n:Nat => n @zero => Nat.zero @succ k => k;
Box := @\n:Nat => {mk:(k:Nat)->* k;};
Reverse := @\n:Nat => @\b:Box (pred n) => {
	mk:(k:Nat)->(b:Box (pred k))->* k b;
};
