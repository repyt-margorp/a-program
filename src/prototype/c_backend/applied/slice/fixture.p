Nat := @{zero : *; succ : * -> *;};
List := \A : @ => @{nil : *; cons : A -> * -> *;};
empty := (List Nat).nil;

take := \xs : List Nat => xs
	@nil => (\limit : Nat => (List Nat).nil)
	@cons head tail => (\limit : Nat => limit
		@zero => (List Nat).nil
		@succ count => (List Nat).cons head (*tail count));
drop := \xs : List Nat => xs
	@nil => (\limit : Nat => (List Nat).nil)
	@cons head tail => (\limit : Nat => limit
		@zero => (List Nat).cons head (*tail Nat.zero)
		@succ count => *tail count);
slice := \xs : List Nat => \offset : Nat => \count : Nat => take (drop xs offset) count;
length := \xs : List Nat => xs @nil => #0 @cons head tail => #int_add #1 *tail;
