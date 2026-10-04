import Bool;
import Nat;
import List;
import natLessOrEqual;
import insertionSortBy;
import insertionSort;
import insertBy;
import quickSort;

bool_type := Bool;
nat_type := Nat;
empty := (List Nat).nil;
sort := \xs : List Nat => insertionSortBy Nat &natLessOrEqual xs;
sort :: List Nat -> List Nat;
sort_alias := \xs : List Nat => insertionSort xs;
insert := \value : Nat => \xs : List Nat => insertBy Nat &natLessOrEqual value xs;
generic_id := \value_type : @ => \value : value_type => value;
fixed_id := \value : Nat => generic_id Nat value;
fixed_int32 := \value : #Int32 => generic_id #Int32 value;
fixed_int64 := \value : #Int64 => generic_id #Int64 value;
fixed_bool := \value : Bool => generic_id Bool value;
compare := \left : Nat => \right : Nat => natLessOrEqual left right;
list_id := \xs : List Nat => xs;
fixed_list_generic := \xs : List Nat => generic_id (List Nat) xs;
type_result := \n : Nat => Nat;
unused_text := \n : #Int32 => (\value_type : @ => \value : #Int32 => value) #Text n;
foreign_type := @{unit : *;};
unused_unselected := \n : #Int32 => (\value_type : @ => \value : #Int32 => value) foreign_type n;
dynamic := \predicate : Nat -> Nat -> Bool => \xs : List Nat => insertionSortBy Nat predicate xs;
effect := \xs : List Nat => { #print #"demanded"; sort xs; };
sort_quick := \xs : List Nat => quickSort Nat &natLessOrEqual xs;

count := \n : Nat => n @zero => #0 @succ prior => #int_add #1 *prior;
fingerprint := \xs : List Nat => xs @nil => #0
	@cons n tail => #int_add (#int_mul #5 *tail) (#int_add #1 (count n));
print_list := \xs : List Nat => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text (count n)); #print #","; *tail; };
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
three := Nat.succ two;
mixed := (List Nat).cons three ((List Nat).cons zero ((List Nat).cons two ((List Nat).cons one ((List Nat).cons two empty))));
descending := (List Nat).cons three ((List Nat).cons two ((List Nat).cons one ((List Nat).cons zero empty)));
reference := {
	print_list (sort empty);
	print_list (sort mixed);
	print_list (sort descending);
	print_list (sort_alias mixed);
	print_list (insert two (sort mixed));
	print_list (insert zero empty);
	#print (#int_to_text (count (fixed_id three))); #print #"|";
	compare two three @true => #print #"T|" @false => #print #"F|";
	compare three two @true => #print #"T|" @false => #print #"F|";
};
