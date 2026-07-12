/* Bool and Nat */
Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

/* Parametric List */
List := \A : @ => @{
	nil  : *;
	cons : A -> * -> *;
};

/* Placeholder equality: always returns false (for demo) */
//is_equal := \A : @ => \l : A => \r : A => Bool.true;
is_equal := \A : @ => \l : A => \r : A => Bool.false;

/* Non-dependent exists_in_list (checks only head due to no recursion yet) */
exists_in_list := \A : @ => \a : A => \lst : List A =>
				lst @nil => Bool.false
				    @cons x xs =>
				        (is_equal A x a
				                @true => Bool.true
				                @false => Bool.false);

/* Example list: [0, 1, 2] */
lst_base := (List Nat).nil;
lst_two := (List Nat).cons (Nat.succ (Nat.succ Nat.zero)) lst_base;
lst_one := (List Nat).cons (Nat.succ Nat.zero) lst_two;
lst := (List Nat).cons Nat.zero lst_one;

/* Query: does 0 exist in lst? (with our is_equal, this is false) */
result := exists_in_list Nat Nat.zero lst;

/* Drive main to show evaluation result */
main := result;
