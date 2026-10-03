import Bool;
import Nat;
import Numbers;
import nat_less_or_equal;
import lower;
import upper;

to_int := \n : Nat => n @zero => #0 @succ prior => #int_add #1 *prior;
report_list := \xs : Numbers => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text (to_int n)); #print #","; *tail; };
report := \xs : Numbers => \pivot : Nat => {
	report_list (lower xs pivot); report_list (upper xs pivot);
};
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
three := Nat.succ two;
sample := Numbers.cons two (Numbers.cons zero (Numbers.cons three (Numbers.cons one (Numbers.cons two Numbers.nil))));
main := {
	report Numbers.nil zero;
	report sample zero; report sample one; report sample two; report sample three;
	#print (#int_to_text (nat_less_or_equal three two @false => #0 @true => #1));
	#print (#int_to_text (nat_less_or_equal two two @false => #0 @true => #1));
	#print (#int_to_text (nat_less_or_equal one two @false => #0 @true => #1));
};
