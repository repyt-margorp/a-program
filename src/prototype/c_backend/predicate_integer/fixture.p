Bool := @{false : *; true : *;};
Reverse := @{true : *; false : *;};
Tri := @{a : *; b : *; c : *;};
Numbers32 := @{nil : *; cons : #Int32 -> * -> *;};
Numbers64 := @{cons : * -> #Int64 -> *; nil : *;};

apply32 := \predicate : #Int32 -> Bool => \n : #Int32 => predicate n;
choose32 := \predicate : #Int32 -> #Int32 -> Bool => \left : #Int32 => \right : #Int32 => predicate left right;
apply64 := \predicate : #Int64 -> Bool => \n : #Int64 => predicate n;
choose64 := \predicate : #Int64 -> #Int64 -> Bool => \left : #Int64 => \right : #Int64 => predicate left right;
reverse32 := \predicate : #Int32 -> Reverse => \n : #Int32 => predicate n;
unused32 := \predicate : #Int32 -> Bool => \n : #Int32 => Bool.true;
filter32 := \predicate : #Int32 -> Bool => \xs : Numbers32 => xs @nil => Numbers32.nil
	@cons n tail => (predicate n @false => *tail @true => Numbers32.cons n *tail);
select32 := \predicate : #Int32 -> #Int32 -> Bool => \xs : Numbers32 => \pivot : #Int32 => xs @nil => Numbers32.nil
	@cons n tail => (predicate n pivot @false => *tail @true => Numbers32.cons n *tail);
filter64 := \predicate : #Int64 -> Bool => \xs : Numbers64 => xs @nil => Numbers64.nil
	@cons tail n => (predicate n @false => *tail @true => Numbers64.cons *tail n);
select64 := \predicate : #Int64 -> #Int64 -> Bool => \xs : Numbers64 => \pivot : #Int64 => xs @nil => Numbers64.nil
	@cons tail n => (predicate n pivot @false => *tail @true => Numbers64.cons *tail n);
length32 := \xs : Numbers32 => xs @nil => #0 @cons n tail => #int_add #1 *tail;
length64 := \xs : Numbers64 => xs @nil => #0 @cons tail n => #int_add #1 *tail;
post_check32 := \predicate : #Int32 -> Bool => \xs : Numbers32 => \n : #Int32 => {
	result := filter32 predicate xs;
	predicate n @false => result @true => result;
};
keep32 := \n : #Int32 => Bool.true;
drop32 := \n : #Int32 => Bool.false;
keep64 := \n : #Int64 => Bool.true;
drop64 := \n : #Int64 => Bool.false;
left32 := \flag : Bool => \left : #Int32 => \right : #Int32 => flag;
left64 := \flag : Bool => \left : #Int64 => \right : #Int64 => flag;
reverse_true32 := \n : #Int32 => Reverse.true;
known_keep32 := \xs : Numbers32 => filter32 &keep32 xs;
known_drop32 := \xs : Numbers32 => filter32 &drop32 xs;
known_keep64 := \xs : Numbers64 => filter64 &keep64 xs;
known_drop64 := \xs : Numbers64 => filter64 &drop64 xs;
captured_filter32 := \flag : Bool => \xs : Numbers32 => filter32 &(\n : #Int32 => flag) xs;
captured_filter64 := \flag : Bool => \xs : Numbers64 => filter64 &(\n : #Int64 => flag) xs;
captured_select32 := \flag : Bool => \xs : Numbers32 => \pivot : #Int32 => select32 &(left32 flag) xs pivot;
captured_select64 := \flag : Bool => \xs : Numbers64 => \pivot : #Int64 => select64 &(left64 flag) xs pivot;
direct_keep32 := \xs : Numbers32 => xs @nil => Numbers32.nil
	@cons n tail => (keep32 n @false => *tail @true => Numbers32.cons n *tail);
direct_drop32 := \xs : Numbers32 => xs @nil => Numbers32.nil
	@cons n tail => (drop32 n @false => *tail @true => Numbers32.cons n *tail);
direct_keep64 := \xs : Numbers64 => xs @nil => Numbers64.nil
	@cons tail n => (keep64 n @false => *tail @true => Numbers64.cons *tail n);
direct_drop64 := \xs : Numbers64 => xs @nil => Numbers64.nil
	@cons tail n => (drop64 n @false => *tail @true => Numbers64.cons *tail n);
direct_filter32 := \flag : Bool => \xs : Numbers32 => xs @nil => Numbers32.nil
	@cons n tail => (flag @false => *tail @true => Numbers32.cons n *tail);
direct_filter64 := \flag : Bool => \xs : Numbers64 => xs @nil => Numbers64.nil
	@cons tail n => (flag @false => *tail @true => Numbers64.cons *tail n);
direct_select32 := \flag : Bool => \xs : Numbers32 => \pivot : #Int32 => xs @nil => Numbers32.nil
	@cons n tail => (left32 flag n pivot @false => *tail @true => Numbers32.cons n *tail);
direct_select64 := \flag : Bool => \xs : Numbers64 => \pivot : #Int64 => xs @nil => Numbers64.nil
	@cons tail n => (left64 flag n pivot @false => *tail @true => Numbers64.cons *tail n);

ternary := \predicate : #Int32 -> #Int32 -> #Int32 -> Bool => \n : #Int32 => predicate n n n;
mixed_width := \predicate : #Int32 -> #Int64 -> Bool => \n : #Int32 => \m : #Int64 => predicate n m;
tri_result := \predicate : #Int32 -> Tri => \n : #Int32 => predicate n;
scalar_result := \predicate : #Int32 -> #Int32 => \n : #Int32 => predicate n;
returned := \predicate : #Int32 -> Bool => predicate;
effect := \predicate : #Int32 -> Bool => \n : #Int32 => { #print #"effect"; predicate n; };
Callables := @{nil : *; cons : (#Int32 -> Bool) -> * -> *;};
callable_identity := \xs : Callables => xs;

sample32 := Numbers32.cons #-1 (Numbers32.cons #0 (Numbers32.cons #1 Numbers32.nil));
print_flag := \flag : Bool => flag @false => #print #"F|" @true => #print #"T|";
print_numbers32 := \xs : Numbers32 => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text n); #print #","; *tail; };
reference := {
	print_flag (apply32 &keep32 #-2147483648);
	print_flag (apply32 &drop32 #2147483647);
	print_flag (choose32 &(left32 Bool.true) #-1 #1);
	print_flag (choose32 &(left32 Bool.false) #1 #-1);
	print_numbers32 (filter32 &keep32 sample32);
	print_numbers32 (filter32 &drop32 sample32);
	print_numbers32 (select32 &(left32 Bool.true) sample32 #0);
	print_numbers32 (select32 &(left32 Bool.false) sample32 #0);
};
reference_known := {
	print_numbers32 (known_keep32 sample32);
	print_numbers32 (known_drop32 sample32);
	print_numbers32 (captured_filter32 Bool.true sample32);
	print_numbers32 (captured_filter32 Bool.false sample32);
	print_numbers32 (captured_select32 Bool.true sample32 #0);
	print_numbers32 (captured_select32 Bool.false sample32 #0);
};
reference_direct := {
	print_numbers32 (direct_keep32 sample32);
	print_numbers32 (direct_drop32 sample32);
	print_numbers32 (direct_filter32 Bool.true sample32);
	print_numbers32 (direct_filter32 Bool.false sample32);
	print_numbers32 (direct_select32 Bool.true sample32 #0);
	print_numbers32 (direct_select32 Bool.false sample32 #0);
};
