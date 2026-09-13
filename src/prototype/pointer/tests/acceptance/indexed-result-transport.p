Nat := @{zero:*; succ:*->*;};
Witness := @\n:Nat => {at:(k:Nat)->* k;};
Edge := @\left:Nat => @\right:Nat => {
	stop:* Nat.zero Nat.zero;
	step:(k:Nat)->* k (Nat.succ k);
	other:(m:Nat)->(k:Nat)->* m (Nat.succ k);
	both:(k:Nat)->* (Nat.succ k) (Nat.succ k);
};
convert := \y:Nat => \n:Nat => \w:Witness n => \edge:Edge y (Nat.succ n) =>
	edge @stop => Nat.zero
		@step k => w
		@other m k => Witness.at m
		@both k => Witness.at (Nat.succ k);
convert :: (y:Nat)->(n:Nat)->Witness n->Edge y (Nat.succ n)->Witness y;
read := \n:Nat => \w:Witness n => w @at k => k;
one := Nat.succ Nat.zero;
expected := one;
main := read one (convert one one (Witness.at one) (Edge.step one));
otherMain := read one (convert one Nat.zero (Witness.at Nat.zero) (Edge.other one Nat.zero));
bothMain := read one (convert one Nat.zero (Witness.at Nat.zero) (Edge.both Nat.zero));
convertFunction := \y:Nat => \n:Nat => \w:Witness n => \edge:Edge y (Nat.succ n) =>
	edge @stop => Nat.zero
		@step k => (\ignored:Nat => w)
		@other m k => (\ignored:Nat => Witness.at m)
		@both k => (\ignored:Nat => Witness.at (Nat.succ k));
convertFunction :: (y:Nat)->(n:Nat)->Witness n->Edge y (Nat.succ n)->Nat->Witness y;
functionMain := read one (convertFunction one one (Witness.at one) (Edge.step one) Nat.zero);
functionOtherMain := read one (convertFunction one Nat.zero (Witness.at Nat.zero) (Edge.other one Nat.zero) Nat.zero);
