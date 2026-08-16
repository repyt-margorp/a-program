Nat := @{
	zero : *;
	succ : * -> *;
};

LE := @\left : Nat => @\right : Nat => {
	zero : (n : Nat) -> * Nat.zero n;
	succ : (m : Nat) -> (n : Nat) -> * m n ->
		* (Nat.succ m) (Nat.succ n);
};

Either := \A : @ => \B : @ => @{
	left : A -> *;
	right : B -> *;
};

compareNat := \left : Nat =>
	left @zero =>
		(\right : Nat =>
			(Either (LE Nat.zero right) (LE right Nat.zero)).left
				(LE.zero right))
		@succ leftPredecessor =>
			(\right : Nat =>
				right @zero =>
					(Either
						(LE (Nat.succ leftPredecessor) Nat.zero)
						(LE Nat.zero (Nat.succ leftPredecessor))).right
						(LE.zero (Nat.succ leftPredecessor))
					@succ rightPredecessor =>
						(*leftPredecessor rightPredecessor
							@left proof =>
								(Either
									(LE (Nat.succ leftPredecessor)
										(Nat.succ rightPredecessor))
									(LE (Nat.succ rightPredecessor)
										(Nat.succ leftPredecessor))).left
									(LE.succ leftPredecessor rightPredecessor proof)
							@right proof =>
								(Either
									(LE (Nat.succ leftPredecessor)
										(Nat.succ rightPredecessor))
									(LE (Nat.succ rightPredecessor)
										(Nat.succ leftPredecessor))).right
									(LE.succ rightPredecessor leftPredecessor proof)));

compareNat :: (left : Nat) -> (right : Nat) ->
	Either (LE left right) (LE right left);

main := compareNat (Nat.succ Nat.zero) Nat.zero;
