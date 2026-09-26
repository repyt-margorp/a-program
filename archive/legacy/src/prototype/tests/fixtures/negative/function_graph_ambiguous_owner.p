Nat := @{
	zero : *;
};

duplicate := \x : Nat => x;
duplicate := \x : Nat => x;
bad := *duplicate Nat.zero;
