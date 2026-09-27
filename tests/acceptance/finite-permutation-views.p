import Nat;
import List;
import Fin;
import Vec;
import permutation;
import permutation_refl;
import list_size;
import list_sized;
import vec_contents;
import vector_refill;
import sized_lookup;
import same_position;
import fin_to_nat;
import position_forward;
import position_left_inverse;
import reordering;
import permutation_reordering;
import reordering_size;
import reordering_positions;
import reordering_observe;
import reordering_observe_back;

general_bridge := permutation_reordering;
general_bridge :: (A:@)->(xs:List A)->(ys:List A)->permutation A xs ys->
	(n:Nat)->(size:list_size A n xs)->reordering A n xs size ys;
general_action := reordering_observe;
general_action :: (A:@)->(n:Nat)->(xs:List A)->(size:list_size A n xs)->(ys:List A)->
	(p:reordering A n xs size ys)->(i:Fin n)->(P:A->@)->
	P (sized_lookup A n ys (reordering_size A n xs size ys p) i)->
	P (sized_lookup A n xs size (position_forward n (reordering_positions A n xs size ys p) i));
general_action_back := reordering_observe_back;
general_action_back :: (A:@)->(n:Nat)->(xs:List A)->(size:list_size A n xs)->(ys:List A)->
	(p:reordering A n xs size ys)->(i:Fin n)->(P:A->@)->
	P (sized_lookup A n xs size (position_forward n (reordering_positions A n xs size ys p) i))->
	P (sized_lookup A n ys (reordering_size A n xs size ys p) i);

zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
three := Nat.succ two;
empty := (List Nat).nil;
singleton := (List Nat).cons one empty;
tail_input := (List Nat).cons two singleton;
tail_output := (List Nat).cons one ((List Nat).cons two empty);
input := (List Nat).cons two ((List Nat).cons one singleton);
middle := (List Nat).cons one tail_input;
output := (List Nat).cons one tail_output;
swapped := (permutation Nat).swap two one singleton;
kept := (permutation Nat).keep one tail_input tail_output ((permutation Nat).swap two one empty);
composed := (permutation Nat).compose input middle output swapped kept;
input_size := list_sized Nat input;
certificate := permutation_reordering Nat input output composed three input_size;
certificate :: reordering Nat three input input_size output;
output_size := reordering_size Nat three input input_size output certificate;
output_size :: list_size Nat three output;
positions := reordering_positions Nat three input input_size output certificate;
first := Fin.zero two;
second := Fin.succ (Fin.zero one);
third := Fin.succ (Fin.succ (Fin.zero zero));
first_origin := fin_to_nat three (position_forward three positions first);
second_origin := fin_to_nat three (position_forward three positions second);
third_origin := fin_to_nat three (position_forward three positions third);
first_value := sized_lookup Nat three output output_size first;
second_value := sized_lookup Nat three output output_size second;
third_value := sized_lookup Nat three output output_size third;
main := vec_contents Nat three (vector_refill Nat three output output_size);
roundtrip := position_left_inverse three positions second;
roundtrip :: same_position three second second;

same_nat := @\left:Nat => @\right:Nat => { refl:(n:Nat)->* n n; };
observed := reordering_observe Nat three input input_size output certificate first
	&(\value:Nat => same_nat one value) (same_nat.refl one);
observed :: same_nat one (sized_lookup Nat three input input_size (position_forward three positions first));
observed_back := reordering_observe_back Nat three input input_size output certificate second
	&(\value:Nat => same_nat one value) (same_nat.refl one);
observed_back :: same_nat one (sized_lookup Nat three output output_size second);
empty_certificate := permutation_reordering Nat empty empty (permutation Nat).nil zero (list_size Nat).nil;
empty_certificate :: reordering Nat zero empty (list_size Nat).nil empty;
singleton_size := list_sized Nat singleton;
singleton_certificate := permutation_reordering Nat singleton singleton (permutation_refl Nat singleton) one singleton_size;
singleton_origin := fin_to_nat one (position_forward one
	(reordering_positions Nat one singleton singleton_size singleton singleton_certificate) (Fin.zero zero));
