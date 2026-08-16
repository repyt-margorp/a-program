Nat := @{
	zero : *;
	succ : * -> *;
};

Weird := \f : Nat -> Nat => \g : Nat -> Nat => @\index : Nat => {
	make : (n : Nat) -> * (g n);
};

residual := \f : Nat -> Nat => \g : Nat -> Nat => \n : Nat =>
	\x : Weird &f &g (f n) => x @make k => Nat.zero;

residual :: (f : Nat -> Nat) -> (g : Nat -> Nat) -> (n : Nat) ->
	Weird &f &g (f n) -> Nat;
