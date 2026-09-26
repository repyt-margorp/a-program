Bool := @{true:*; false:*;};
Nat := @{zero:*; succ:*->*;};
LE := @\left:Nat => @\right:Nat => {
	zero:(n:Nat)->* Nat.zero n;
	succ:(m:Nat)->(n:Nat)->* m n->* (Nat.succ m) (Nat.succ n);
};
natLessOrEqual := \left:Nat => left
	@zero => (\right:Nat => Bool.true)
	@succ leftPredecessor => (\right:Nat => right
		@zero => Bool.false
		@succ rightPredecessor => *leftPredecessor rightPredecessor);
natLessOrEqual :: Nat->Nat->Bool;
comparison_graph := \x:Nat => x @(self => (y:Nat)->@natLessOrEqual self y (natLessOrEqual self y))
	@zero => (\y:Nat => (@natLessOrEqual).case0 y)
	@succ left => (\y:Nat => y @(self => @natLessOrEqual (Nat.succ left) self (natLessOrEqual (Nat.succ left) self))
		@zero => (@natLessOrEqual).case1 left
		@succ right => (@natLessOrEqual).case2 left right (natLessOrEqual left right) (*left right));
comparison_graph :: (x:Nat)->(y:Nat)->@natLessOrEqual x y (natLessOrEqual x y);
Decision := @\x:Nat => @\y:Nat => @\answer:Bool => {
	yes:(a:Nat)->(b:Nat)->LE a b->* a b Bool.true;
	no:(a:Nat)->(b:Nat)->LE (Nat.succ b) a->* a b Bool.false;
};
lift := \x:Nat => \y:Nat => \answer:Bool => \proof:Decision x y answer => proof
	@yes a b prior => Decision.yes (Nat.succ a) (Nat.succ b) (LE.succ a b prior)
	@no a b prior => Decision.no (Nat.succ a) (Nat.succ b) (LE.succ (Nat.succ b) a prior);
lift :: (x:Nat)->(y:Nat)->(answer:Bool)->Decision x y answer->
	Decision (Nat.succ x) (Nat.succ y) answer;
correct := \x:Nat => \y:Nat => \answer:Bool => \proof:@natLessOrEqual x y answer => proof
	@case0 right => Decision.yes Nat.zero right (LE.zero right)
	@case1 left => Decision.no (Nat.succ left) Nat.zero (LE.succ Nat.zero left (LE.zero left))
	@case2 left right answer prior => lift left right answer *prior;
correct :: (x:Nat)->(y:Nat)->(answer:Bool)->@natLessOrEqual x y answer->Decision x y answer;
read := \x:Nat => \y:Nat => \answer:Bool => \proof:Decision x y answer => proof
	@yes a b prior => Bool.true
	@no a b prior => Bool.false;
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
base := read zero two (natLessOrEqual zero two) (correct zero two (natLessOrEqual zero two) (comparison_graph zero two));
greater := read two one (natLessOrEqual two one) (correct two one (natLessOrEqual two one) (comparison_graph two one));
smaller := read one two (natLessOrEqual one two) (correct one two (natLessOrEqual one two) (comparison_graph one two));
same := read two two (natLessOrEqual two two) (correct two two (natLessOrEqual two two) (comparison_graph two two));
trueValue := Bool.true;
falseValue := Bool.false;
