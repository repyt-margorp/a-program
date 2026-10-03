Bool := @{false : *; true : *;};
Nat := @{zero : *; succ : * -> *;};
Numbers := @{nil : *; cons : Nat -> * -> *;};
LongNumbers := @{cons : * -> #Int64 -> *; nil : *;};
Reversed := @{succ : * -> *; zero : *;};
Tree := @{leaf : *; fork : * -> * -> *;};

nat_less_or_equal := \left : Nat => left
	@zero => (\right : Nat => Bool.true)
	@succ left_predecessor => (\right : Nat => right
		@zero => Bool.false
		@succ right_predecessor => *left_predecessor right_predecessor);
nat_less_or_equal :: Nat -> Nat -> Bool;
lower := \xs : Numbers => \pivot : Nat => xs @nil => Numbers.nil
	@cons n tail => (nat_less_or_equal n pivot
		@false => *tail @true => Numbers.cons n *tail);
upper := \xs : Numbers => \pivot : Nat => xs @nil => Numbers.nil
	@cons n tail => (nat_less_or_equal n pivot
		@false => Numbers.cons n *tail @true => *tail);
length := \xs : Numbers => xs @nil => #0 @cons n tail => #int_add #1 *tail;
increment := \n : Nat => Nat.succ n;
reversed_increment := \n : Reversed => Reversed.succ n;
reversed_zero := Reversed.zero;
constant := Numbers.nil;
long_identity := \xs : LongNumbers => xs;
block_callback := \xs : Numbers => { f := \n : Nat => Nat.succ n; f Nat.zero; };
callback := \f : Nat -> Nat => \n : Nat => f n;
effect := \n : Nat => { #print #"unsupported-effect"; Nat.succ n; };
