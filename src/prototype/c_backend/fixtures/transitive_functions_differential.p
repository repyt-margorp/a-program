import chain_three;
import chain_four;
import chain_eight;
import shadowed;
import repeated;
import unused_effect;

main := {
	#print (#int_to_text (chain_three #-2147483648 #-1)); #print #"|";
	#print (#int_to_text (chain_four #2147483647 #1)); #print #"|";
	#print (#int_to_text (chain_eight #42 #-99)); #print #"|";
	#print (#int_to_text (shadowed #7 #-9)); #print #"|";
	#print (#int_to_text (repeated #2147483647 #1)); #print #"|";
	#print (#int_to_text (unused_effect #42 #-99)); #print #"|";
};
