Nat := @{zero:*; succ:*->*;};
LE := @\left:Nat => @\right:Nat => {
	zero:(n:Nat)->* Nat.zero n;
	succ:(m:Nat)->(n:Nat)->* m n->* (Nat.succ m) (Nat.succ n);
};
keep := \a:Nat => \b:Nat => \proof:LE a b => proof;
rebuild := \b:Nat => \z:Nat => \upper:LE (Nat.succ b) z => upper
	@succ c d rest => LE.succ b d (keep b d rest);
rebuild :: (b:Nat)->(z:Nat)->LE (Nat.succ b) z->LE (Nat.succ b) z;
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
expected := LE.succ zero one (LE.zero one);
main := rebuild zero two expected;
