Nat := @{zero:*; succ:*->*;};
Witness := @\n:Nat => {at:(k:Nat)->* k;};
Edge := @\left:Nat => @\right:Nat => {
	step:(k:Nat)->* k (Nat.succ k);
	other:(m:Nat)->(k:Nat)->* m (Nat.succ k);
};
convert := \y:Nat => \n:Nat => \w:Witness n => \edge:Edge y (Nat.succ n) =>
	edge @step k => w
		@other m k => Witness.at m;
// The result index is y, not the unrelated n. This post-check must fail.
convert :: (y:Nat)->(n:Nat)->Witness n->Edge y (Nat.succ n)->Witness n;
