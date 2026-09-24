// Append to either universal theorem, keeping its nominal provider unchanged.
bool_order := @\left:Bool => @\right:Bool => {
	bottom:(b:Bool)->* Bool.false b;
	top:* Bool.true Bool.true;
};
bool_le := \x:Bool => x
	@false => (\y:Bool => Bool.true)
	@true => (\y:Bool => y);
bool_refl := \x:Bool => x
	@false => bool_order.bottom Bool.false
	@true => bool_order.top;
bool_trans := \x:Bool => \y:Bool => \p:bool_order x y => p
	@bottom b => (\z:Bool => \q:bool_order b z => bool_order.bottom z)
	@top => (\z:Bool => \q:bool_order Bool.true z => q);
bool_trans :: (x:Bool)->(y:Bool)->bool_order x y->(z:Bool)->bool_order y z->bool_order x z;
bool_decide := \x:Bool => \y:Bool => x
	@(self => general_decision Bool &bool_order self y (bool_le self y))
	@false => (general_decision Bool &bool_order Bool.false y).yes (bool_order.bottom y)
	@true => (y @(self => general_decision Bool &bool_order Bool.true self (bool_le Bool.true self))
		@false => (general_decision Bool &bool_order Bool.true Bool.false).no (bool_order.bottom Bool.true)
		@true => (general_decision Bool &bool_order Bool.true Bool.true).yes bool_order.top);
bool_decide :: (x:Bool)->(y:Bool)->general_decision Bool &bool_order x y (bool_le x y);
bool_sorted := general_sorted Bool &bool_order;
bool_correct := quick_correct Bool &bool_le &bool_order &bool_trans &bool_refl &bool_decide;
read_bool_sorted := \xs:List Bool => \proof:bool_sorted xs => proof
	@nil => Nat.zero
	@cons head tail bound rest => Nat.succ *rest;
checked_length := \xs:List Bool => *quickSort Bool &bool_le xs @ys =>
	read_bool_sorted ys (bool_correct xs ys @ys);
checked_value := \xs:List Bool => *quickSort Bool &bool_le xs @ys => ys;
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
