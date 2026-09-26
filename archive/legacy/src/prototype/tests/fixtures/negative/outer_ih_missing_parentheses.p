Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

List := \A : @ => @{
	nil : *;
	cons : A -> * -> *;
};

invalid := \xs : List Nat =>
	xs @nil => (List Nat).nil
		@cons head tail =>
			Bool.true
				@true => (List Nat).cons head (List Nat).nil
				@false => *tail;
