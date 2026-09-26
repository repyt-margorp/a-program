Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
natLessOrEqual := \left : Nat => left
	@zero => (\right : Nat => Bool.true)
	@succ leftPredecessor => (\right : Nat => right
		@zero => Bool.false
		@succ rightPredecessor => *leftPredecessor rightPredecessor);
natLessOrEqual :: Nat -> Nat -> Bool;
comparison_graph := \x:Nat => x @(self => (y:Nat)->@natLessOrEqual self y (natLessOrEqual self y))
	@zero => (\y:Nat => (@natLessOrEqual).case0 y)
	@succ left => (\y:Nat => y @(self => @natLessOrEqual (Nat.succ left) self (natLessOrEqual (Nat.succ left) self))
		@zero => (@natLessOrEqual).case1 left
		@succ right => (@natLessOrEqual).case2 left right (natLessOrEqual left right) (*left right));
comparison_graph :: (x:Nat)->(y:Nat)->@natLessOrEqual x y (natLessOrEqual x y);
// An independent specification of the comparator's recursive equations.
Compared := @\x : Nat => @\y : Nat => @\answer : Bool => {
	zero : (right : Nat) -> * Nat.zero right Bool.true;
	nonzero : (left : Nat) -> * (Nat.succ left) Nat.zero Bool.false;
	both : (left : Nat) -> (right : Nat) -> (answer : Bool) -> * left right answer ->
		* (Nat.succ left) (Nat.succ right) answer;
};
correct := \x : Nat => \y : Nat => \answer : Bool =>
	\proof : @natLessOrEqual x y answer => proof
	@case0 right => Compared.zero right
	@case1 left => Compared.nonzero left
	@case2 left right answer prior => Compared.both left right answer *prior;
correct :: (x : Nat) -> (y : Nat) -> (answer : Bool) ->
	@natLessOrEqual x y answer -> Compared x y answer;
read := \x : Nat => \y : Nat => \b : Bool => \p : Compared x y b => p
	@zero right => Bool.true @nonzero left => Bool.false
	@both left right answer prior => *prior;
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
base := read zero two (natLessOrEqual zero two) (correct zero two (natLessOrEqual zero two) (comparison_graph zero two));
greater := read two one (natLessOrEqual two one) (correct two one (natLessOrEqual two one) (comparison_graph two one));
smaller := read one two (natLessOrEqual one two) (correct one two (natLessOrEqual one two) (comparison_graph one two));
same := read two two (natLessOrEqual two two) (correct two two (natLessOrEqual two two) (comparison_graph two two));
trueValue := Bool.true;
falseValue := Bool.false;
