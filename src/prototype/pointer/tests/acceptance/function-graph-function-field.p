Nat := @{zero:*; succ:*->*;};
D := @\i:Nat => {mk:(k:Nat)->* k; next:((k:Nat)->* k)->* Nat.zero;};
steps := \i:Nat => \v:D i => v
	@mk k => Nat.zero
	@next down => Nat.succ (*down Nat.zero);
steps_graph := \i:Nat => \v:D i => v @(index self => @steps index self (steps index self))
	@mk k => (@steps).mk k
	@next down => (@steps).next &down (steps Nat.zero (down Nat.zero)) (*down Nat.zero);
steps_graph :: (i:Nat)->(v:D i)->@steps i v (steps i v);
sample := D.next &(\k:Nat => D.mk k);
main := steps Nat.zero sample;
expected := Nat.succ Nat.zero;

from := \i:Nat => \v:D i => v
	@mk k => (\start:Nat => start)
	@next down => (\start:Nat => *down start (Nat.succ start));
fromGraph := @from;
fromMain := from Nat.zero sample Nat.zero;

Tree := @{leaf:*; node:(Nat->*)->*;};
walk := \t:Tree => t
	@leaf => (\start:Nat => Nat.succ start)
	@node down => (\start:Nat => {first := *down Nat.zero start; *down first first;});
select := \n:Nat => n
	@zero => Tree.leaf
	@succ k => Tree.node &(\j:Nat => Tree.leaf);
tree := Tree.node &select;
walkGraph := @walk;
walkMain := walk tree Nat.zero;
three := Nat.succ (Nat.succ expected);

SameNat := @\left:Nat => @\right:Nat => {
	zero:* Nat.zero Nat.zero;
	succ:(a:Nat)->(b:Nat)->* a b->* (Nat.succ a) (Nat.succ b);
};
stepsCorrect := \i:Nat => \v:D i => \n:Nat => \g:@steps i v n => g
	@mk k => SameNat.zero
	@next down childOutput childGraph => SameNat.succ childOutput childOutput *childGraph;
stepsCorrect :: (i:Nat)->(v:D i)->(n:Nat)->(@steps i v n)->SameNat n n;
readSame := \a:Nat => \b:Nat => \proof:SameNat a b => proof
	@zero => Nat.zero
	@succ x y rest => Nat.succ *rest;
proofMain := readSame (steps Nat.zero sample) (steps Nat.zero sample)
	(stepsCorrect Nat.zero sample (steps Nat.zero sample) (steps_graph Nat.zero sample));

Branch := @{leaf:*; node:((k:Nat)->SameNat k k->*)->*;};
visit := \b:Branch => b @leaf => Nat.zero
	@node down => Nat.succ (*down Nat.zero SameNat.zero);
visitGraph := @visit;
visitMain := visit (Branch.node &(\k:Nat => \p:SameNat k k => Branch.leaf));
