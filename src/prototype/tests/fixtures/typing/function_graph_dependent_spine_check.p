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

certified := {
	packet := *headOr Bool Bool.false sample;
	packet @returned output graph => inspect Bool Bool.false sample output graph;
};

main := headOr Bool Bool.false sample;
expected := { Bool.true; };
