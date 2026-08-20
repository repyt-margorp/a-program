Nat := @{
	zero : *;
	succ : * -> *;
};

NatList := @{
	nil : *;
	cons : Nat -> * -> *;
};

Sorted := @\xs : NatList => {
	nilSorted : * NatList.nil;
	oneSorted : (x : Nat) -> * (NatList.cons x NatList.nil);
};

Bool := @{
	true : *;
	false : *;
};

choose := \decision : Bool => \left : NatList => \right : NatList =>
	decision
		@true => left
		@false => right;

choose :: Bool -> NatList -> NatList -> NatList;

chooseComputation := &{
	choose Bool.true NatList.nil (NatList.cons Nat.zero NatList.nil);
};
chooseResult := NatList.nil;
chooseReturns := #.returns (chooseComputation) chooseResult;
chooseReturns :: #.Returns (chooseComputation) chooseResult;

consumeResult := \result : NatList =>
	\evaluation : #.Returns (chooseComputation) result =>
	\proof : Sorted result => proof;

certifiedChoose := consumeResult chooseResult chooseReturns Sorted.nilSorted;
certifiedChoose :: Sorted chooseResult;

consumeOpenResult := \decision : Bool => \result : NatList =>
	\evaluation : #.Returns (&(choose decision NatList.nil
		(NatList.cons Nat.zero NatList.nil))) result =>
	\proof : Sorted result => proof;
