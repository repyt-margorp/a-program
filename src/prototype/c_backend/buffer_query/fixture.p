Bool := @{true : *; false : *;};
Nat := @{zero : *; succ : * -> *;};
Packet := @{empty : *; small : #Int32 -> Bool -> *; wide : #Int64 -> *;};
Numbers32 := @{cons : #Int32 -> * -> *; nil : *;};
Numbers64 := @{nil : *; cons : * -> #Int64 -> *;};
Flags := @{nil : *; cons : * -> Bool -> *;};
Records := @{nil : *; cons : Packet -> * -> *;};
Nats := @{nil : *; cons : Nat -> * -> *;};
Multi := @{nil : *; cons : #Int32 -> Bool -> * -> *;};
Tree := @{leaf : #Int32 -> *; branch : * -> * -> *;};
Callables := @{nil : *; cons : (#Int32 -> #Int32) -> * -> *;};
Indexed := @\flag : Bool => {empty : * Bool.false;};

length32 := \xs : Numbers32 => xs @nil => #0 @cons n tail => #int_add #1 *tail;
sum32 := \xs : Numbers32 => xs @nil => #0 @cons n tail => #int_add n *tail;
append32 := \xs : Numbers32 => \ys : Numbers32 => xs @nil => ys @cons n tail => Numbers32.cons n *tail;
take32 := \xs : Numbers32 => xs @nil => (\limit : Nat => Numbers32.nil)
	@cons n tail => (\limit : Nat => limit @zero => Numbers32.nil @succ count => Numbers32.cons n (*tail count));
drop32 := \xs : Numbers32 => xs @nil => (\limit : Nat => Numbers32.nil)
	@cons n tail => (\limit : Nat => limit @zero => Numbers32.cons n (*tail Nat.zero) @succ count => *tail count);
slice32 := \xs : Numbers32 => \offset : Nat => \count : Nat => take32 (drop32 xs offset) count;
identity32 := \xs : Numbers32 => xs;
identity64 := \xs : Numbers64 => xs;
identity_flags := \xs : Flags => xs;
identity_records := \xs : Records => xs;
identity_nats := \xs : Nats => xs;
identity_multi := \xs : Multi => xs;
identity_tree := \xs : Tree => xs;
identity_callable := \xs : Callables => xs;
identity_indexed := \xs : Indexed Bool.false => xs;
identity_bool := \flag : Bool => flag;
callback := \f : #Int32 -> #Int32 => \n : #Int32 => f n;
effect := \xs : Numbers32 => { #print #"effect"; sum32 xs; };

sample := Numbers32.cons #-1 (Numbers32.cons #0 (Numbers32.cons #1 Numbers32.nil));
print_numbers := \xs : Numbers32 => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text n); #print #","; *tail; };
reference := {
	print_numbers (take32 sample (Nat.succ Nat.zero));
	print_numbers (drop32 sample (Nat.succ Nat.zero));
	print_numbers (slice32 sample (Nat.succ Nat.zero) (Nat.succ Nat.zero));
	#print (#int_to_text (sum32 sample)); #print #"|";
};
