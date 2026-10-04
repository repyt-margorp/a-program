Bool := @{false : *; true : *;};
Numbers32 := @{nil : *; cons : #Int32 -> * -> *;};
Numbers64 := @{cons : * -> #Int64 -> *; nil : *;};
filter32 := \predicate : #Int32 -> Bool => \xs : Numbers32 => xs @nil => Numbers32.nil
	@cons n tail => (predicate n @false => *tail @true => Numbers32.cons n *tail);
select32 := \predicate : #Int32 -> #Int32 -> Bool => \xs : Numbers32 => \pivot : #Int32 => xs @nil => Numbers32.nil
	@cons n tail => (predicate n pivot @false => *tail @true => Numbers32.cons n *tail);
filter64 := \predicate : #Int64 -> Bool => \xs : Numbers64 => xs @nil => Numbers64.nil
	@cons tail n => (predicate n @false => *tail @true => Numbers64.cons *tail n);
select64 := \predicate : #Int64 -> #Int64 -> Bool => \xs : Numbers64 => \pivot : #Int64 => xs @nil => Numbers64.nil
	@cons tail n => (predicate n pivot @false => *tail @true => Numbers64.cons *tail n);
keep32 := \n : #Int32 => Bool.true;
drop32 := \n : #Int32 => Bool.false;
keep64 := \n : #Int64 => Bool.true;
drop64 := \n : #Int64 => Bool.false;
left32 := \flag : Bool => \left : #Int32 => \right : #Int32 => flag;
left64 := \flag : Bool => \left : #Int64 => \right : #Int64 => flag;
known_keep32 := \xs : Numbers32 => filter32 &keep32 xs;
known_drop32 := \xs : Numbers32 => filter32 &drop32 xs;
known_keep64 := \xs : Numbers64 => filter64 &keep64 xs;
known_drop64 := \xs : Numbers64 => filter64 &drop64 xs;
captured_filter32 := \flag : Bool => \xs : Numbers32 => filter32 &(\n : #Int32 => flag) xs;
captured_filter64 := \flag : Bool => \xs : Numbers64 => filter64 &(\n : #Int64 => flag) xs;
captured_select32 := \flag : Bool => \xs : Numbers32 => \pivot : #Int32 => select32 &(left32 flag) xs pivot;
captured_select64 := \flag : Bool => \xs : Numbers64 => \pivot : #Int64 => select64 &(left64 flag) xs pivot;
length32 := \xs : Numbers32 => xs @nil => #0 @cons n tail => #int_add #1 *tail;
length64 := \xs : Numbers64 => xs @nil => #0 @cons tail n => #int_add #1 *tail;
sample32 := Numbers32.cons #-1 (Numbers32.cons #0 (Numbers32.cons #1 Numbers32.nil));
print_numbers32 := \xs : Numbers32 => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text n); #print #","; *tail; };

chain_filter32 := \flag : Bool => \ignored : Bool => \xs : Numbers32 => {
	f := \n : #Int32 => flag;
	g := \n : #Int32 => f n;
	filter32 &(\n : #Int32 => g n) xs;
};
shadow_filter32 := \flag : Bool => \other : Bool => \xs : Numbers32 => {
	f := \n : #Int32 => flag;
	(\flag : Bool => filter32 &(\n : #Int32 => f n) xs) other;
};
map32 := \transform : #Int32 -> #Int32 => \xs : Numbers32 => xs @nil => Numbers32.nil
	@cons n tail => Numbers32.cons (transform n) *tail;
map64 := \transform : #Int64 -> #Int64 => \xs : Numbers64 => xs @nil => Numbers64.nil
	@cons tail n => Numbers64.cons *tail (transform n);
shift32 := \offset : #Int32 => \xs : Numbers32 => map32 &(\n : #Int32 => #int_add n offset) xs;
shift64 := \offset : #Int64 => \xs : Numbers64 => map64 &(\n : #Int64 => #int64_add n offset) xs;
chain_map32 := \first : #Int32 => \second : #Int32 => \xs : Numbers32 => {
	f := \n : #Int32 => #int_add n first;
	g := \n : #Int32 => #int_add (f n) second;
	map32 &(\n : #Int32 => g n) xs;
};
unused_effect := \flag : Bool => \xs : Numbers32 => {
	f := \n : #Int32 => { #print #"unreached"; #int_add n #1; };
	filter32 &(\n : #Int32 => flag) xs;
};
demanded_effect := \xs : Numbers32 => { #print #"demanded"; map32 &(\n : #Int32 => #int_add n #1) xs; };
dynamic_map := \transform : #Int32 -> #Int32 => \xs : Numbers32 => map32 transform xs;

Nat := @{zero : *; succ : * -> *;};
Naturals := @{nil : *; cons : Nat -> * -> *;};
keep_nat := \n : Nat => n @zero => Bool.false @succ prior => Bool.true;
less_equal_nat := \left : Nat => left
	@zero => (\right : Nat => Bool.true)
	@succ prior_left => (\right : Nat => right
		@zero => Bool.false @succ prior_right => *prior_left prior_right);
less_equal_nat :: Nat -> Nat -> Bool;
filter_nat := \predicate : Nat -> Bool => \xs : Naturals => xs @nil => Naturals.nil
	@cons n tail => (predicate n @false => *tail @true => Naturals.cons n *tail);
select_nat := \predicate : Nat -> Nat -> Bool => \xs : Naturals => \pivot : Nat => xs @nil => Naturals.nil
	@cons n tail => (predicate n pivot @false => *tail @true => Naturals.cons n *tail);
known_filter_nat := \xs : Naturals => filter_nat &keep_nat xs;
known_select_nat := \xs : Naturals => \pivot : Nat => select_nat &less_equal_nat xs pivot;
count_nat := \n : Nat => n @zero => #0 @succ prior => #int_add #1 *prior;
fingerprint_nat := \xs : Naturals => xs @nil => #0
	@cons n tail => #int_add (#int_mul #5 *tail) (#int_add #1 (count_nat n));
print_naturals := \xs : Naturals => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text (count_nat n)); #print #","; *tail; };
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
natural_sample := Naturals.cons zero (Naturals.cons two (Naturals.cons one (Naturals.cons two Naturals.nil)));
reference := {
	print_numbers32 (known_keep32 sample32);
	print_numbers32 (known_drop32 sample32);
	print_numbers32 (captured_filter32 Bool.true sample32);
	print_numbers32 (captured_filter32 Bool.false sample32);
	print_numbers32 (captured_select32 Bool.true sample32 #0);
	print_numbers32 (captured_select32 Bool.false sample32 #0);
	print_numbers32 (chain_filter32 Bool.true Bool.false sample32);
	print_numbers32 (chain_filter32 Bool.false Bool.true sample32);
	print_numbers32 (shadow_filter32 Bool.true Bool.false sample32);
	print_numbers32 (shadow_filter32 Bool.false Bool.true sample32);
	print_numbers32 (shift32 #2147483647 sample32);
	print_numbers32 (chain_map32 #2147483647 #1 sample32);
	print_numbers32 (unused_effect Bool.true sample32);
	print_naturals (known_filter_nat natural_sample);
	print_naturals (known_select_nat natural_sample one);
	print_naturals (known_select_nat natural_sample two);
};
