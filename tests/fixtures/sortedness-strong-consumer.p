// Both independently constructed witnesses inhabit the same strong family.
new_correct := quick_strongly_sorted Bool &bool_order &bool_le &bool_trans &bool_decide;
old_correct := quick_correct_existing Bool &bool_order &bool_le &bool_trans &bool_refl &bool_decide;
read_strong := \xs:List Bool => \proof:general_strongly_sorted Bool &bool_order xs => proof
	@nil => (List Bool).nil
	@cons h t ph pt => (List Bool).cons h *pt;
read_local_bool := \xs:List Bool => \proof:general_locally_sorted Bool &bool_order xs => proof
	@nil => (List Bool).nil
	@one x => (List Bool).cons x (List Bool).nil
	@cons x y ys edge tail => (List Bool).cons x *tail;
nil := (List Bool).nil;
singleton := (List Bool).cons Bool.true nil;
sample := (List Bool).cons Bool.true ((List Bool).cons Bool.false nil);
expected := (List Bool).cons Bool.false singleton;
output := quickSort Bool &bool_le sample;
new_value := read_strong output (new_correct sample);
old_value := read_strong output (old_correct sample);
local_value := read_local_bool output
	(strongly_sorted_to_locally_sorted Bool &bool_order output (new_correct sample));
empty_value := read_strong (quickSort Bool &bool_le nil) (new_correct nil);
singleton_value := read_strong (quickSort Bool &bool_le singleton) (new_correct singleton);
