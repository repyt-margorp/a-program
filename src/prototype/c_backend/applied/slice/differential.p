import Nat;
import List;
import take;
import drop;
import slice;
to_int := \n : Nat => n @zero => #0 @succ tail => #int_add #1 *tail;
report := \xs : List Nat => xs
	@nil => #print #"|"
	@cons head tail => {
		#print (#int_to_text (to_int head));
		#print #",";
		*tail;
	};
one := Nat.succ Nat.zero;
two := Nat.succ one;
four := Nat.succ (Nat.succ two);
sample := (List Nat).cons Nat.zero ((List Nat).cons one ((List Nat).cons two (List Nat).nil));
main := {
	report (take sample Nat.zero);
	report (take sample two);
	report (take sample four);
	report (drop sample Nat.zero);
	report (drop sample one);
	report (drop sample four);
	report (slice sample one one);
	report (slice sample one four);
	report (slice sample four two);
};
