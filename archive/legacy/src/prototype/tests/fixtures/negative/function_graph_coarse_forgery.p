Nat := @{
	zero : *;
};

first := \left : Nat => \right : Nat => left;
first :: Nat -> Nat -> Nat;

// The historical fixture makes no false claim: the direct graph is inhabited.
forgedByCoarseFallback := (@first Nat.zero).case0 Nat.zero;
forgedByCoarseFallback :: @first Nat.zero Nat.zero Nat.zero;
