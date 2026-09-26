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
Decision := @\x:Nat => @\y:Nat => @\answer:Bool => {
	yes:(a:Nat)->(b:Nat)->LE a b->* a b Bool.true;
	no:(a:Nat)->(b:Nat)->LE (Nat.succ b) a->* a b Bool.false;
};
lift := \x:Nat => \y:Nat => \answer:Bool => \proof:Decision x y answer => proof
	@yes a b prior => Decision.yes (Nat.succ a) (Nat.succ b) (LE.succ a b prior)
	@no a b prior => Decision.no (Nat.succ a) (Nat.succ b) (LE.succ (Nat.succ b) a prior);
correct := \x:Nat => \y:Nat => \answer:Bool => \proof:@natLessOrEqual x y answer => proof
	@case0 right => Decision.yes Nat.zero right (LE.zero right)
	@case1 left => Decision.no (Nat.succ left) Nat.zero (LE.succ Nat.zero left (LE.zero left))
	@case2 left right answer prior => lift left right answer *prior;
// True for x <= y cannot be asserted as true for y <= x.
correct :: (x:Nat)->(y:Nat)->(answer:Bool)->@natLessOrEqual x y answer->Decision y x answer;
