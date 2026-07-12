/* Common Lisp-style null predicate written with the project match syntax. */

Bool := @{
	true : *;
	false : *;
};

List := \A : @ => @{
	nil  : *;
	cons : A -> * -> *;
};

null := \lst : List Bool =>
	lst @nil => Bool.true
	    @cons x xs => Bool.false;

empty := (List Bool).nil;
one := (List Bool).cons Bool.true empty;

main := null empty;
