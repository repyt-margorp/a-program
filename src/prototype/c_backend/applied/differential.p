import Nat;
import List;
import length;
import append;
import prepend;
report := \xs : List Nat => {
	#print (#int_to_text (length xs)); #print #" ";
	#print (#int_to_text (length (append xs xs))); #print #" ";
	#print (#int_to_text (length (prepend (Nat.succ Nat.zero) xs))); #print #"|";
};
main := {
	report (List Nat).nil;
	report ((List Nat).cons (Nat.succ (Nat.succ Nat.zero)) ((List Nat).cons Nat.zero ((List Nat).cons (Nat.succ Nat.zero) (List Nat).nil)));
	report ((List Nat).cons (Nat.succ Nat.zero) ((List Nat).cons (Nat.succ Nat.zero) (List Nat).nil));
};
