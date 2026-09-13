Nat := @{zero:*; succ:*->*;};
Tree := @{leaf:*; node:(Nat->*)->*;};
depth := \t:Tree => t @leaf => Nat.zero
	@node down => Nat.succ (*down Nat.zero);
Graph := @depth;
down := \n:Nat => Tree.node &(\m:Nat => Tree.leaf);
// The actual child is another node, not the leaf certified by this premise.
wrong := Graph.node &down Nat.zero Graph.leaf;
