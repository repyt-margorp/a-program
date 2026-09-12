Nat := @{ zero : *; succ : * -> *; };
Tree := @{
	leaf : *;
	branch : (Nat -> *) -> *;
	telescope : ((A : @) -> A -> *) -> *;
	mixed : (Nat -> *) -> * -> *;
};
depth := \tree : Tree =>
	tree @leaf => Nat.zero
	     @branch down => Nat.succ (*down Nat.zero)
	     @telescope down => Nat.succ (*down Nat Nat.zero)
	     @mixed down direct => Nat.succ (*down *direct);
one := Tree.branch &(\n : Nat => Tree.leaf);
two := Tree.telescope &(\A : @ => \x : A => one);
main := depth two;
expected := Nat.succ (Nat.succ Nat.zero);
main :: Nat;
mixed := depth (Tree.mixed &(\n : Nat => one) Tree.leaf);
Indexed := @\n : Nat => {
	at : (k : Nat) -> * k;
	fan : ((k : Nat) -> * k) -> * Nat.zero;
};
steps := \n : Nat => \v : Indexed n =>
	v @at k => Nat.zero
	  @fan down => Nat.succ (*down (Nat.succ Nat.zero));
indexed := steps Nat.zero (Indexed.fan &(\k : Nat => Indexed.at k));
indexedExpected := Nat.succ Nat.zero;
