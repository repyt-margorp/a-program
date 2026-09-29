Nat := @{ zero : *; succ : * -> *; };
List := \A : @ => @{ nil : *; cons : A -> * -> *; };
len := \A : @ => \xs : List A =>
	xs @nil => Nat.zero @cons value tail => Nat.succ *tail;
toInt := \n : Nat => n @zero => #0 @succ k => #int_add #1 *k;
main := #print (#int_to_text (toInt (len #Int ((List #Int).cons #42 (List #Int).nil))));
