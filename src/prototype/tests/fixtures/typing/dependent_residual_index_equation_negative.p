Nat := @{
	zero : *;
	succ : * -> *;
};

At := @\index : Nat => {
	at : (n : Nat) -> * n;
};

Weird := \f : Nat -> Nat => \g : Nat -> Nat => @\index : Nat => {
	make : (n : Nat) -> At (g n) -> * (g n);
};

dependentResidual := \f : Nat -> Nat => \g : Nat -> Nat => \n : Nat =>
	\x : Weird f g (f n) => x @make k value => value;

dependentResidual :: (f : Nat -> Nat) -> (g : Nat -> Nat) -> (n : Nat) ->
	Weird f g (f n) -> At (f n);
