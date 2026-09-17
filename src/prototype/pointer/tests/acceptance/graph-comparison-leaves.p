Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
natLessOrEqual := \left : Nat => left
	@zero => (\right : Nat => Bool.true)
	@succ leftPredecessor => (\right : Nat => right
		@zero => Bool.false
		@succ rightPredecessor => *leftPredecessor rightPredecessor);
natLessOrEqual :: Nat -> Nat -> Bool;
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
base := *natLessOrEqual zero two @answer => read zero two answer (correct zero two answer @answer);
greater := *natLessOrEqual two one @answer => read two one answer (correct two one answer @answer);
smaller := *natLessOrEqual one two @answer => read one two answer (correct one two answer @answer);
same := *natLessOrEqual two two @answer => read two two answer (correct two two answer @answer);
trueValue := Bool.true;
falseValue := Bool.false;
