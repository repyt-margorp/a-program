Nat := @{zero:*; succ:*->*;};
Bool := @{true:*; false:*;};
Chain := @\n:Nat => {zero:* Nat.zero; succ:(k:Nat)->* k->* (Nat.succ k);};
Witness := @\n:Nat => {at:(k:Nat)->* k;};
next := \k:Nat => \decision:Bool => \proof:Witness k => Witness.at (Nat.succ k);

make := \n:Nat => \chain:Chain n =>
	chain @zero => Witness.at Nat.zero
		@succ k rest => {
			decision := Bool.true;
			next k decision *rest;
		};
make :: (n:Nat)->Chain n->Witness n;

nested := \n:Nat => \chain:Chain n =>
	chain @zero => Witness.at Nat.zero
		@succ k rest => {
			choose := \b:Bool => b;
			{
				decision := choose Bool.true;
				Bool.false;
				next k decision *rest;
			};
		};
nested :: (n:Nat)->Chain n->Witness n;

selected := \n:Nat => \chain:Chain n =>
	chain @zero => Witness.at Nat.zero
		@succ k rest => {
			result := next k Bool.true *rest;
			unreachable := missing;
		}.result;
selected :: (n:Nat)->Chain n->Witness n;

one := Nat.succ Nat.zero;
two := Nat.succ one;
input := Chain.succ one (Chain.succ Nat.zero Chain.zero);
expected := Witness.at two;
main := make two input;
nestedMain := nested two input;
selectedMain := selected two input;
readMain := (make two input) @at k => k;
