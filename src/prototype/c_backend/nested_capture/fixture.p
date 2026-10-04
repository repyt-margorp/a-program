Bool := @{false : *; true : *;};
Nat := @{zero : *; succ : * -> *;};
count := \n : Nat => n @zero => #0 @succ prior => #int_add #1 *prior;
branch_sum := \flag : Bool => \n : Nat => n @zero => #0
	@succ prior => (flag @false => *prior @true => #int_add #1 *prior);
nested_sum := \n : Nat => n @zero => #0
	@succ prior => (n @zero => *prior @succ inner => #int_add #1 *inner);
nested_known := \n : Nat => n @zero => #0 @succ prior => {
	f := \ignored : Nat => *prior;
	n @zero => f Nat.zero @succ inner => #int_add #1 *inner;
};
nested_seed32 := \seed : #Int32 => \n : Nat => n @zero => seed
	@succ prior => (n @zero => *prior @succ inner => #int_add #1 *inner);
nested_seed64 := \seed : #Int64 => \n : Nat => n @zero => seed
	@succ prior => (n @zero => *prior @succ inner => #int64_add seed *inner);
nested_curried := \n : Nat => n @zero => (\seed : #Int32 => seed)
	@succ prior => (\seed : #Int32 => n
		@zero => *prior seed @succ inner => #int_add #1 *inner);
Numbers := @{nil : *; cons : #Int32 -> * -> *;};
nested_list := \xs : Numbers => xs @nil => #0
	@cons n tail => (Nat.succ Nat.zero
		@zero => *tail @succ previous => #int_add n *previous);
nested_unused := \n : Nat => n @zero => #0
	@succ prior => (Nat.zero @zero => #7 @succ inner => *prior);
nested_twice := \n : Nat => n @zero => #0
	@succ prior => (n @zero => #int_add *prior *prior @succ inner => #int_add #1 *inner);
dynamic := \predicate : Nat -> #Int32 => \n : Nat => predicate n;
demanded_effect := \n : Nat => { #print #"demanded"; nested_sum n; };
Callable := @{wrap : (Nat -> Nat) -> *;};
callable_identity := \value : Callable => value;
Indexed := @\size : Nat => {
	nil : * Nat.zero;
	cons : (n : Nat) -> * n -> * (Nat.succ n);
};
indexed_type := Indexed Nat.zero;
indexed_identity := \value : Indexed Nat.zero => value;
one := Nat.succ Nat.zero;
two := Nat.succ one;
three := Nat.succ two;
reference := {
	#print (#int_to_text (count three));
	#print #",";
	#print (#int_to_text (branch_sum Bool.true three));
	#print #",";
	#print (#int_to_text (branch_sum Bool.false three));
	#print #",";
	#print (#int_to_text (nested_sum three));
	#print #",";
	#print (#int_to_text (nested_known three));
	#print #",";
	#print (#int_to_text (nested_seed32 #-1 three));
	#print #",";
	#print (#int_to_text (nested_curried three #-1));
	#print #",";
	#print (#int_to_text (nested_list (Numbers.cons #-1 (Numbers.cons #2 Numbers.nil))));
};
