// Append to either universal theorem, keeping its nominal provider unchanged.
import quick_correct_existing;
bool_sorted := general_sorted Bool &bool_order;
graph_bool_correct := quick_correct Bool &bool_le &bool_order &bool_trans &bool_refl &bool_decide;
bool_correct := quick_correct_existing Bool &bool_order &bool_le &bool_trans &bool_refl &bool_decide;
read_bool_sorted := \xs:List Bool => \proof:bool_sorted xs => proof
	@nil => Nat.zero
	@cons head tail bound rest => Nat.succ *rest;
checked_length := \xs:List Bool => read_bool_sorted (quickSort Bool &bool_le xs) (bool_correct xs);
checked_value := \xs:List Bool => quickSort Bool &bool_le xs;
nil := (List Bool).nil;
singleton := (List Bool).cons Bool.true nil;
ordered := (List Bool).cons Bool.false singleton;
reverse := (List Bool).cons Bool.true ((List Bool).cons Bool.false nil);
duplicates := (List Bool).cons Bool.true ((List Bool).cons Bool.false ordered);
duplicates_expected := (List Bool).cons Bool.false ((List Bool).cons Bool.false
	((List Bool).cons Bool.true singleton));
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
four := Nat.succ (Nat.succ two);
empty_length := checked_length nil;
singleton_length := checked_length singleton;
ordered_length := checked_length ordered;
reverse_length := checked_length reverse;
duplicates_length := checked_length duplicates;
empty_value := checked_value nil;
singleton_value := checked_value singleton;
ordered_value := checked_value ordered;
reverse_value := checked_value reverse;
duplicates_value := checked_value duplicates;
direct_value := quickSort Bool &bool_le duplicates;
