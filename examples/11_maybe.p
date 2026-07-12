Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

Maybe := \A : @ => @{
	nothing : *;
	just : A -> *;
};

fromMaybe := \A : @ => \default : A => \m : Maybe A =>
	m @nothing => default
	  @just x => x;

main := fromMaybe Nat Nat.zero ((Maybe Nat).just (Nat.succ Nat.zero));
