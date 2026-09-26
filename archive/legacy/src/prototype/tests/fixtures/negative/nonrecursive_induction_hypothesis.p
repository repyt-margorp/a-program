Nat := @{
	zero : *;
	succ : * -> *;
};

Tree := @{
	leaf : Nat -> *;
	fork : * -> * -> *;
};

invalid := \tree : Tree =>
	tree @leaf value => *value
		@fork left right => Nat.zero;
