Bool := @{false : *; true : *;};
Packet := @{empty : *; small : #Int32 -> Bool -> *; wide : #Int64 -> #Int32 -> *;};
Envelope := @{none : *; pair : Packet -> #Int32 -> *;};
Tree := @{leaf : Envelope -> *; fork : * -> Envelope -> * -> *;};
Reverse := @{fork : * -> * -> Envelope -> *; leaf : Envelope -> *;};
Single := @{leaf : Envelope -> *; more : Envelope -> * -> *;};
Wide := @{leaf : Envelope -> *; fork : * -> * -> * -> *;};
Box := @{box : Tree -> *;};
Aggregate := @{leaf : Box -> *; fork : * -> * -> *;};
Callable := @{leaf : *; fork : (#Int32 -> *) -> * -> *;};
Indexed := @\flag : Bool => {empty : * Bool.false;};
empty_indexed := Indexed.empty;

number := \p : Packet => p @empty => #0
	@small n b => (b @false => n @true => #int_add n #1)
	@wide n m => m;
measure := \e : Envelope => e @none => #0 @pair p n => #int_add (number p) n;
identity := \tree : Tree => tree;
sum := \tree : Tree => tree @leaf e => measure e
	@fork left e right => #int_add (#int_add *left (measure e)) *right;
mirror := \tree : Tree => tree @leaf e => Tree.leaf e
	@fork left e right => Tree.fork *right e *left;
leaf := \e : Envelope => Tree.leaf e;
fork := \left : Tree => \e : Envelope => \right : Tree => Tree.fork left e right;
wide_packet := \n : #Int64 => \m : #Int32 => Packet.wide n m;
envelope := \p : Packet => \n : #Int32 => Envelope.pair p n;
reverse_identity := \tree : Reverse => tree;
reverse_sum := \tree : Reverse => tree @fork left right e => #int_add (#int_add *left (measure e)) *right
	@leaf e => measure e;
reverse_mirror := \tree : Reverse => tree @fork left right e => Reverse.fork *right *left e
	@leaf e => Reverse.leaf e;
effect := \tree : Tree => { #print #"unsupported-effect"; sum tree; };
callback := \f : #Int32 -> #Int32 => \tree : Tree => f (sum tree);
single_identity := \tree : Single => tree;
wide_identity := \tree : Wide => tree;
aggregate_identity := \tree : Aggregate => tree;
callable_identity := \tree : Callable => tree;

e1 := Envelope.pair (Packet.small #7 Bool.true) #3;
e2 := Envelope.pair Packet.empty #5;
sample := Tree.fork (Tree.leaf e1) e2 (Tree.leaf Envelope.none);
sample_reverse := Reverse.fork (Reverse.leaf e1) (Reverse.leaf Envelope.none) e2;
reference := {
	#print (#int_to_text (sum sample)); #print (#int_to_text (sum (mirror sample)));
	#print (#int_to_text (reverse_sum sample_reverse));
	#print (#int_to_text (reverse_sum (reverse_mirror sample_reverse)));
};
