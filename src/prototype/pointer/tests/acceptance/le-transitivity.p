// Conventional LE transitivity, using the ordinary indexed IH.
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
trans :: (x:Nat)->(y:Nat)->LE x y->(z:Nat)->LE y z->LE x z;
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
main := trans one one (LE.succ zero zero (LE.zero zero)) two (LE.succ zero one (LE.zero one));
expected := LE.succ zero one (LE.zero one);
base := trans zero one (LE.zero one) two (LE.succ zero one (LE.zero one));
baseExpected := LE.zero two;
deep := trans two two (LE.succ one one (LE.succ zero zero (LE.zero zero))) two
	(LE.succ one one (LE.succ zero zero (LE.zero zero)));
deepExpected := LE.succ one one (LE.succ zero zero (LE.zero zero));
