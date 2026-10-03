import Nat;
import Bool;
import Numbers;
import apply_predicate;
import apply_comparator;
import filter;
import select;
import length;
import keep;
import zero;
import one;
import two;
import sample;
import print_flag;
import print_numbers;

module_choose := \left : Nat => left
	@zero => (\right : Nat => Bool.true)
	@succ prior => (\right : Nat => keep right);
module_choose :: Nat -> Nat -> Bool;
n3 := Nat.succ two;
n4 := Nat.succ n3;
n5 := Nat.succ n4;
n6 := Nat.succ n5;
n7 := Nat.succ n6;
n8 := Nat.succ n7;
observe_right := \left : Nat => \right : Nat => right
	@zero => print_flag (apply_comparator &module_choose left zero)
	@succ prior => { *prior; print_flag (apply_comparator &module_choose left right); };
observe := \left : Nat => {
	print_flag (apply_predicate &keep left);
	observe_right left n8;
};
observe_all := \n : Nat => n @zero => observe zero
	@succ prior => { *prior; observe n; };
module_reference := {
	observe_all n8;
	print_numbers (filter &keep sample);
	print_numbers (select &module_choose sample zero);
	print_numbers (select &module_choose sample one);
};
