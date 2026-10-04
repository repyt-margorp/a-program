Bool := @{false : *; true : *;};
Nat := @{zero : *; succ : * -> *;};
Numbers32 := @{nil : *; cons : #Int32 -> * -> *;};
Numbers64 := @{cons : * -> #Int64 -> *; nil : *;};
Tree32 := @{leaf : #Int32 -> *; branch : * -> * -> *;};
Packet := @{packet : #Int32 -> Bool -> *;};

map32 := \f : #Int32 -> #Int32 => \xs : Numbers32 => xs @nil => Numbers32.nil
	@cons n tail => Numbers32.cons (f n) *tail;
map64 := \f : #Int64 -> #Int64 => \xs : Numbers64 => xs @nil => Numbers64.nil
	@cons tail n => Numbers64.cons *tail (f n);
combine32 := \f : #Int32 -> #Int32 -> #Int32 => \xs : Numbers32 => \operand : #Int32 => xs @nil => Numbers32.nil
	@cons n tail => Numbers32.cons (f n operand) *tail;
combine64 := \f : #Int64 -> #Int64 -> #Int64 => \xs : Numbers64 => \operand : #Int64 => xs @nil => Numbers64.nil
	@cons tail n => Numbers64.cons *tail (f n operand);
reduce32 := \f : #Int32 -> #Int32 -> #Int32 => \xs : Numbers32 => \seed : #Int32 => xs @nil => seed
	@cons n tail => f n *tail;
reduce64 := \f : #Int64 -> #Int64 -> #Int64 => \xs : Numbers64 => \seed : #Int64 => xs @nil => seed
	@cons tail n => f n *tail;
tree32 := \leaf : #Int32 -> #Int32 => \branch : #Int32 -> #Int32 -> #Int32 => \xs : Tree32 => xs
	@leaf n => leaf n @branch left right => branch *left *right;
packet32 := \f : #Int32 -> #Int32 => \n : #Int32 => \flag : Bool => Packet.packet (f n) flag;
length32 := \xs : Numbers32 => xs @nil => #0 @cons n tail => #int_add #1 *tail;
length64 := \xs : Numbers64 => xs @nil => #0 @cons tail n => #int_add #1 *tail;
apply32 := \f : #Int32 -> #Int32 => \n : #Int32 => f n;
apply64 := \f : #Int64 -> #Int64 => \n : #Int64 => f n;
unused32 := \f : #Int32 -> #Int32 => \n : #Int32 => n;
negate32 := \n : #Int32 => #int_neg n;
negate64 := \n : #Int64 => #int64_neg n;
subtract32 := \left : #Int32 => \right : #Int32 => #int_sub left right;
subtract64 := \left : #Int64 => \right : #Int64 => #int64_sub left right;

ternary := \f : #Int32 -> #Int32 -> #Int32 -> #Int32 => \n : #Int32 => f n n n;
mixed_width := \f : #Int32 -> #Int64 -> #Int64 => \n : #Int32 => \m : #Int64 => f n m;
mixed_result := \f : #Int32 -> #Int64 => \n : #Int32 => f n;
enum_result := \f : #Int32 -> Bool => \n : #Int32 => f n;
natural_domain := \f : Nat -> Nat => \n : Nat => f n;
returned := \f : #Int32 -> #Int32 => f;
effect := \f : #Int32 -> #Int32 => \n : #Int32 => { #print #"effect"; f n; };
Callables := @{nil : *; cons : (#Int32 -> #Int32) -> * -> *;};
callable_identity := \xs : Callables => xs;
Indexed := @\flag : Bool => {empty : * Bool.false;};
indexed_identity := \xs : Indexed Bool.false => xs;

sample32 := Numbers32.cons #-1 (Numbers32.cons #0 (Numbers32.cons #1 Numbers32.nil));
sample_tree := Tree32.branch (Tree32.leaf #-1) (Tree32.branch (Tree32.leaf #0) (Tree32.leaf #1));
print_numbers := \xs : Numbers32 => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text n); #print #","; *tail; };
reference := {
	print_numbers (map32 &negate32 sample32);
	print_numbers (combine32 &subtract32 sample32 #1);
	#print (#int_to_text (reduce32 &subtract32 sample32 #0)); #print #"|";
	#print (#int_to_text (reduce32 &subtract32 sample32 #-2147483648)); #print #"|";
	#print (#int_to_text (tree32 &negate32 &subtract32 sample_tree)); #print #"|";
};
