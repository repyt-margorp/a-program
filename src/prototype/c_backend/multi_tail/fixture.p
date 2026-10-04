Bool := @{false : *; true : *;};
Tree := @{leaf : #Int32 -> *; fork : Bool -> * -> * -> *;};
Reverse := @{fork : * -> Bool -> * -> *; leaf : #Int32 -> *;};
Callable := @{leaf : *; fork : (#Int32 -> *) -> *;};
Wide := @{leaf : *; fork : * -> * -> * -> *;};
Indexed := @\flag : Bool => {empty : * Bool.false;};
empty_indexed := Indexed.empty;
Record := @{value : #Int32 -> *;};
RecordTree := @{leaf : Record -> *; fork : * -> * -> *;};

identity := \tree : Tree => tree;
sum := \tree : Tree => tree @leaf value => value
	@fork flag left right => #int_add *left *right;
fingerprint := \tree : Tree => tree @leaf value => #int_add #17 value
	@fork flag left right => #int_add (#int_mul #3 *left)
		(#int_add (#int_mul #5 *right) (flag @false => #7 @true => #11));
mirror := \tree : Tree => tree @leaf value => Tree.leaf value
	@fork flag left right => Tree.fork flag *right *left;
shift := \tree : Tree => \delta : #Int32 => tree
	@leaf value => Tree.leaf (#int_add value delta)
	@fork flag left right => Tree.fork flag *left *right;
leaf := \value : #Int32 => Tree.leaf value;
fork := \flag : Bool => \left : Tree => \right : Tree => Tree.fork flag left right;
reverse_identity := \tree : Reverse => tree;
reverse_sum := \tree : Reverse => tree @fork left flag right => #int_add *left *right
	@leaf value => value;
reverse_fingerprint := \tree : Reverse => tree @fork left flag right =>
	#int_add (#int_mul #3 *left) (#int_add (#int_mul #5 *right) (flag @false => #7 @true => #11))
	@leaf value => #int_add #17 value;
reverse_mirror := \tree : Reverse => tree @fork left flag right => Reverse.fork *right flag *left
	@leaf value => Reverse.leaf value;
effect := \tree : Tree => { #print #"unsupported-effect"; sum tree; };
callback := \f : #Int32 -> #Int32 => \tree : Tree => f (sum tree);
wide_identity := \tree : Wide => tree;
callable_identity := \tree : Callable => tree;
record_identity := \tree : RecordTree => tree;

sample := Tree.fork Bool.true (Tree.leaf #2)
	(Tree.fork Bool.false (Tree.leaf #3) (Tree.leaf #4));
sample_reverse := Reverse.fork (Reverse.leaf #2) Bool.true
	(Reverse.fork (Reverse.leaf #3) Bool.false (Reverse.leaf #4));
reference := {
	#print (#int_to_text (sum sample)); #print (#int_to_text (sum (mirror sample)));
	#print (#int_to_text (sum (shift sample #1))); #print (#int_to_text (sum (identity sample)));
	#print (#int_to_text (reverse_sum sample_reverse));
	#print (#int_to_text (reverse_sum (reverse_mirror sample_reverse)));
};
