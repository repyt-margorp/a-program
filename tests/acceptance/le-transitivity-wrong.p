Nat := @{zero:*; succ:*->*;};
LE := @\left:Nat => @\right:Nat => {
	zero:(n:Nat)->* Nat.zero n;
	succ:(m:Nat)->(n:Nat)->* m n->* (Nat.succ m) (Nat.succ n);
};
step := \a:Nat => \b:Nat => \ih:(z:Nat)->LE b z->LE a z =>
	\z:Nat => \upper:LE (Nat.succ b) z => upper
		@succ c d rest => LE.succ a d (ih d rest);
trans := \x:Nat => \y:Nat => \p:LE x y => p
	@zero n => (\z:Nat => \upper:LE n z => LE.zero z)
	@succ a b prior => step a b &*prior;
// A checked induction theorem cannot be reclassified with reversed endpoints.
trans :: (x:Nat)->(y:Nat)->LE x y->(z:Nat)->LE y z->LE z x;
