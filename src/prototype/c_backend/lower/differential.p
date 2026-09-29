import nested;
import partial;
import through_fold;
import nested_capture;
import shadowed;
import scoped;

main := {
	#print (#int_to_text (nested #-2147483648 #65537)); #print #"|";
	#print (#int_to_text (partial #2147483647)); #print #"|";
	#print (#int_to_text (through_fold #2147483647)); #print #"|";
	#print (#int_to_text (nested_capture #-99)); #print #"|";
	#print (#int_to_text (shadowed #99)); #print #"|";
	#print (#int_to_text (scoped #-99)); #print #"|";
};
