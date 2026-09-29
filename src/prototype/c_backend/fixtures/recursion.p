Nat := @{ zero : *; succ : * -> *; };
List := @{ nil : *; cons : #Int -> * -> *; };
sum := \xs : List =>
	xs @nil => #0
	   @cons x tail => #int_add x *tail;
toInt := \n : Nat => n @zero => #0 @succ k => #int_add #1 *k;
main := {
	#print (#int_to_text (sum (List.cons #10 (List.cons #20 List.nil))));
	#print #":";
	#print (#int_to_text (toInt (Nat.succ (Nat.succ Nat.zero))));
};
Other := @{ zero : *; succ : * -> *; };
otherToText := \n : Other => n @zero => #"other" @succ k => *k;
nominal := #print (otherToText (Other.succ Other.zero));
