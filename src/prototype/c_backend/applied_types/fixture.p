Bool := @{false : *; true : *;};
Nat := @{zero : *; succ : * -> *;};
List := \A : @ => @{nil : *; cons : A -> * -> *;};
Pair := \A : @ => \B : @ => @{none : *; pair : A -> B -> *;};
Sized := \A : @ => @\n : Nat => {nil : * Nat.zero;};
empty := (List Nat).nil;
list_type := List Nat;
empty_flags := (List Bool).nil;
empty_pair := (Pair Nat Bool).none;
empty_sized := (Sized Nat).nil;
generic_id := \value_type : @ => \value : value_type => value;
captured_id := \value_type : @ => \value : value_type =>
	(\marker : @ => generic_id value_type value) Bool;
direct_list := \xs : List Nat => xs;
fixed_nat := \n : Nat => generic_id Nat n;
fixed_list := \xs : List Nat => generic_id (List Nat) xs;
alias_list := \xs : List Nat => generic_id list_type xs;
map_inline := \xs : List Nat => xs @nil => empty
	@cons n tail => generic_id (List Nat) ((List Nat).cons n *tail);
map_list := \xs : List Nat => xs @nil => empty @cons n tail => {
	copied := (List Nat).cons n *tail;
	generic_id (List Nat) copied;
};
shadow_list := \xs : List Nat => captured_id (List Nat) xs;
fixed_pair := \p : Pair Nat Bool => generic_id (Pair Nat Bool) p;
fixed_flags := \xs : List Bool => generic_id (List Bool) xs;
unused_flags := \n : Nat => (\value_type : @ => \value : Nat => value) (List Bool) n;
unused_open := \n : Nat => (\value_type : (@ -> @) => \value : Nat => value) List n;
unused_other_pair := \n : Nat => (\value_type : @ => \value : Nat => value) (Pair #Int32 Bool) n;
type_result := \n : Nat => List Nat;
effect := \xs : List Nat => { #print #"demanded"; fixed_list xs; };
count := \n : Nat => n @zero => #0 @succ prior => #int_add #1 *prior;
length := \xs : List Nat => xs @nil => #0 @cons n tail => #int_add #1 *tail;
fingerprint := \xs : List Nat => xs @nil => #0
	@cons n tail => #int_add (#int_mul #5 *tail) (#int_add #1 (count n));
pair_code := \p : Pair Nat Bool => p @none => #0 @pair n b =>
	#int_add (#int_mul #2 (count n)) (b @false => #1 @true => #2);
sample := (List Nat).cons Nat.zero ((List Nat).cons (Nat.succ Nat.zero) empty);
reference := {
	#print (#int_to_text (count (fixed_nat (Nat.succ Nat.zero))));
	#print (#int_to_text (length (fixed_list sample)));
	#print (#int_to_text (length (alias_list sample)));
	#print (#int_to_text (length (map_list sample)));
	#print (#int_to_text (length (map_inline sample)));
	#print (#int_to_text (length (shadow_list sample)));
	#print (#int_to_text (pair_code (fixed_pair empty_pair)));
	#print (#int_to_text (pair_code (fixed_pair ((Pair Nat Bool).pair (Nat.succ Nat.zero) Bool.true))));
};
