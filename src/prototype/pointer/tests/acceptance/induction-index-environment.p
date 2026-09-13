Nat := @{zero:*; succ:*->*;};
Chain := @\n:Nat => {zero:* Nat.zero; succ:(k:Nat)->* k->* (Nat.succ k);};
Witness := @\n:Nat => {at:(k:Nat)->* k;};
keep := \n:Nat => \p:Chain n =>
	p @zero => p
		@succ k rest => (\unused:Chain k => p) *rest;
keep :: (n:Nat)->Chain n->Chain n;
readWitness := \n:Nat => \w:Witness n => w @at k => k;
read := \n:Nat => \w:Witness n => \p:Chain n =>
	p @zero => readWitness Nat.zero w
		@succ k rest => Nat.succ (*rest (Witness.at k));
read :: (n:Nat)->Witness n->Chain n->Nat;
one := Nat.succ Nat.zero;
two := Nat.succ one;
chainOne := Chain.succ Nat.zero Chain.zero;
expected := Chain.succ one chainOne;
main := keep two expected;
readMain := read two (Witness.at two) main;
