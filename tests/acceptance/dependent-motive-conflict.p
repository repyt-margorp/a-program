Nat := @{zero : *; succ : * -> *;};
Bool := @{false : *; true : *;};
Tree := @{leaf : *; left : * -> *; right : * -> *;};
negate := \b : Bool => b @false => Bool.true @true => Bool.false;

// The two recursive uses impose incompatible equations on the same motive.
bad := \tree : Tree => tree
	@leaf => Nat.zero
	@left child => Nat.succ *child
	@right child => negate *child;
