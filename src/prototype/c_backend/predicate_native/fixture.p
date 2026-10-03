Bool := @{false : *; true : *;};
Reverse := @{true : *; false : *;};
Tri := @{a : *; b : *; c : *;};
Nat := @{zero : *; succ : * -> *;};
OtherNat := @{zero : *; succ : * -> *;};
Numbers := @{nil : *; cons : Nat -> * -> *;};
Callables := @{nil : *; cons : (Nat -> Bool) -> * -> *;};

apply_predicate := \predicate : Nat -> Bool => \n : Nat => predicate n;
apply_comparator := \predicate : Nat -> Nat -> Bool => \left : Nat => \right : Nat => predicate left right;
apply_reverse := \predicate : Nat -> Reverse => \n : Nat => predicate n;
other_reverse := \predicate : OtherNat -> Reverse => \n : OtherNat => predicate n;
unused := \predicate : Nat -> Bool => \n : Nat => Bool.true;
filter := \predicate : Nat -> Bool => \xs : Numbers => xs @nil => Numbers.nil
	@cons n tail => (predicate n @false => *tail @true => Numbers.cons n *tail);
select := \predicate : Nat -> Nat -> Bool => \xs : Numbers => \pivot : Nat => xs @nil => Numbers.nil
	@cons n tail => (predicate n pivot @false => *tail @true => Numbers.cons n *tail);
post_check := \predicate : Nat -> Bool => \xs : Numbers => \n : Nat => {
	result := filter predicate xs;
	predicate n @false => result @true => result;
};
length := \xs : Numbers => xs @nil => #0 @cons n tail => #int_add #1 *tail;
count_nat := \n : Nat => n @zero => #0 @succ prior => #int_add #1 *prior;
fingerprint := \xs : Numbers => xs @nil => #0
	@cons n tail => #int_add (#int_mul #5 *tail) (#int_add #1 (count_nat n));
increment := \n : Nat => Nat.succ n;
keep := \n : Nat => n @zero => Bool.false @succ prior => Bool.true;
less_equal := \left : Nat => left
	@zero => (\right : Nat => Bool.true)
	@succ prior_left => (\right : Nat => right
		@zero => Bool.false @succ prior_right => *prior_left prior_right);
less_equal :: Nat -> Nat -> Bool;

ternary := \predicate : Nat -> Nat -> Nat -> Bool => \n : Nat => predicate n n n;
mixed_domains := \predicate : Nat -> OtherNat -> Bool => \n : Nat => \m : OtherNat => predicate n m;
tri_result := \predicate : Nat -> Tri => \n : Nat => predicate n;
host_domain := \predicate : #Int32 -> Bool => \n : #Int32 => predicate n;
host_result := \predicate : Nat -> #Int32 => \n : Nat => predicate n;
returned := \predicate : Nat -> Bool => predicate;
effect := \predicate : Nat -> Bool => \n : Nat => { #print #"effect"; predicate n; };
callable_identity := \xs : Callables => xs;

zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
sample := Numbers.cons zero (Numbers.cons one (Numbers.cons two (Numbers.cons zero Numbers.nil)));
print_flag := \flag : Bool => flag @false => #print #"F|" @true => #print #"T|";
print_numbers := \xs : Numbers => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text (count_nat n)); #print #","; *tail; };
reference := {
	print_flag (apply_predicate &keep zero);
	print_flag (apply_predicate &keep two);
	print_flag (apply_comparator &less_equal one two);
	print_flag (apply_comparator &less_equal two one);
	print_numbers (filter &keep sample);
	print_numbers (select &less_equal sample one);
};
