import Bool;
import List;
import length;
import sum;
import append;
import select;
import composed;

report := \xs : List => {
	#print (#int_to_text (length xs)); #print #" ";
	#print (#int_to_text (sum xs)); #print #" ";
	#print (#int_to_text (composed xs)); #print #" ";
	#print (#int_to_text (length (select xs Bool.false))); #print #" ";
	#print (#int_to_text (sum (select xs Bool.false))); #print #" ";
	#print (#int_to_text (length (select xs Bool.true))); #print #" ";
	#print (#int_to_text (sum (select xs Bool.true))); #print #"|";
};
one := List.cons #-2147483648 Bool.false List.nil;
two := List.cons #2147483647 Bool.true one;
three := List.cons #1 Bool.true two;
four := List.cons #-1 Bool.false three;
main := { report List.nil; report one; report two; report three; report four; };
