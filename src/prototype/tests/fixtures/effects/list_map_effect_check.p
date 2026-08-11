Nat := @{
	zero : *;
	succ : * -> *;
};

List := \A : @ => @{
	nil  : *;
	cons : A -> * -> *;
};

map := \A : @ => \B : @ => \transform : A -> B => \xs : List A =>
	xs @nil => (List B).nil
	   @cons head tail => {
		mappedHead := transform head;
		mappedTail := *tail;
		result := (List B).cons mappedHead mappedTail;
	};

effectfulSuccessor := \value : Nat => {
	#.print #"x";
	result := Nat.succ value;
};
effectfulSuccessor :: Nat -> Nat;

sample := (List Nat).cons Nat.zero
	((List Nat).cons (Nat.succ Nat.zero) (List Nat).nil);

main := map Nat Nat &effectfulSuccessor sample;
