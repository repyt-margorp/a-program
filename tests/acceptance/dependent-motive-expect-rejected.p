Nat := @{zero : *; succ : * -> *;};
Bool := @{false : *; true : *;};
Tree := @{leaf : *; next : * -> *;};

// An expectation cannot override the APP domain constraint Nat.
bad := \tree : Tree => tree
	@leaf => Nat.zero
	@next child => Nat.succ (*child :: Bool);
