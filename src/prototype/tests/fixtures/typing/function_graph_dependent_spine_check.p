Bool := @{
	true : *;
	false : *;
};

List := \A : @ => @{
	nil : *;
	cons : A -> * -> *;
};

headOr := \A : @ => \fallback : A => \xs : List A =>
	xs
		@nil => fallback
		@cons head tail => head;

headOr :: (A : @) -> A -> List A -> A;

inspect := \A : @ => \fallback : A => \input : List A => \output : A =>
	\graph : @headOr A fallback input output =>
		graph
			@nil => output
			@cons head tail => output;

sample := (List Bool).cons Bool.true (List Bool).nil;

head_graph := \A:@ => \fallback:A => \xs:List A => xs
	@(self => @headOr A fallback self (headOr A fallback self))
	@nil => (@headOr A fallback).nil
	@cons h t => (@headOr A fallback).cons h t;
head_graph :: (A:@)->(fallback:A)->(xs:List A)->@headOr A fallback xs (headOr A fallback xs);
certified := inspect Bool Bool.false sample (headOr Bool Bool.false sample) (head_graph Bool Bool.false sample);

main := headOr Bool Bool.false sample;
expected := { Bool.true; };
