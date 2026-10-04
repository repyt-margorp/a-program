import Bool;
import Numbers32;
import Numbers64;
import filter32;
import filter64;
import select32;
import select64;
import known_keep32;
import known_drop32;
import known_keep64;
import known_drop64;
import captured_filter32;
import captured_filter64;
import captured_select32;
import captured_select64;
import length32;
import length64;
import sample32;
import print_numbers32;

public_bool := Bool;
public_numbers32 := Numbers32;
public_numbers64 := Numbers64;
public_known_keep32 := \xs : Numbers32 => known_keep32 xs;
public_known_drop32 := \xs : Numbers32 => known_drop32 xs;
public_known_keep64 := \xs : Numbers64 => known_keep64 xs;
public_known_drop64 := \xs : Numbers64 => known_drop64 xs;
public_captured_filter32 := \flag : Bool => \xs : Numbers32 => captured_filter32 flag xs;
public_captured_filter64 := \flag : Bool => \xs : Numbers64 => captured_filter64 flag xs;
public_captured_select32 := \flag : Bool => \xs : Numbers32 => \pivot : #Int32 => captured_select32 flag xs pivot;
public_captured_select64 := \flag : Bool => \xs : Numbers64 => \pivot : #Int64 => captured_select64 flag xs pivot;
public_length32 := \xs : Numbers32 => length32 xs;
public_length64 := \xs : Numbers64 => length64 xs;

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
