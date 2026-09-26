Nat := @{
	zero : *;
	succ : * -> *;
};

LE := @\left : Nat => @\right : Nat => {
	zero : (n : Nat) -> * Nat.zero n;
	succ : (m : Nat) -> (n : Nat) -> * m n ->
		* (Nat.succ m) (Nat.succ n);
};

OrderedFrom := @\lower : Nat => {
	nil : (current : Nat) -> * current;
	cons : (current : Nat) -> (head : Nat) -> LE current head -> * head ->
		* current;
};

rebuild := \lower : Nat => \xs : OrderedFrom lower =>
	xs @nil current => OrderedFrom.nil lower
		@cons current head currentToHead tail =>
			OrderedFrom.cons lower head
				(currentToHead :: LE lower head)
				*tail;

rebuild :: (lower : Nat) -> OrderedFrom lower -> OrderedFrom lower;

main := rebuild Nat.zero (OrderedFrom.nil Nat.zero);
