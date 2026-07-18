Nat := @{
	zero : *;
	succ : * -> *;
};

dependentIdentity := \A : @ => \x : A => x;
dependentIdentity :: (A : @) -> (x : A) -> A;

main := dependentIdentity Nat Nat.zero;
