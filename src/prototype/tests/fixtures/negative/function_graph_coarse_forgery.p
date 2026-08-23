Nat := @{
	zero : *;
};

first := \left : Nat => \right : Nat => left;
first :: Nat -> Nat -> Nat;

forgedByCoarseFallback := *first Nat.zero Nat.zero;
