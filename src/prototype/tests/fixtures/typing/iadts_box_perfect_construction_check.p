Nat := @{
	zero : *;
	succ : * -> *;
};

Pair := \A : @ => \B : @ => @{
	pair : A -> B -> *;
};

Box := @\contents : @ => {
	pack : (A : @) -> A -> * A;
};

Perfect := @\A : @ => {
	leaf : (B : @) -> B -> * B;
	node : (B : @) -> * (Pair B B) -> * B;
};

natBox := Box.pack Nat Nat.zero;
natBox :: Box Nat;

leafNat := Perfect.leaf Nat Nat.zero;
leafNat :: Perfect Nat;

pairNat := (Pair Nat Nat).pair Nat.zero Nat.zero;
pairLeaf := Perfect.leaf (Pair Nat Nat) pairNat;
nodeNat := Perfect.node Nat pairLeaf;
nodeNat :: Perfect Nat;
