import closed_block;
import captured_block;
import shadowed_block;
import repeated_block;
import curried_block;
import unused_block;
import nested_two;
import unused_effect;

main := {
	#print (#int_to_text (closed_block #2147483647)); #print #"|";
	#print (#int_to_text (captured_block #-2147483648 #-1)); #print #"|";
	#print (#int_to_text (shadowed_block #-2147483648)); #print #"|";
	#print (#int_to_text (repeated_block #2147483647)); #print #"|";
	#print (#int_to_text (curried_block #2147483647)); #print #"|";
	#print (#int_to_text (unused_block #-2147483648)); #print #"|";
	#print (#int_to_text (nested_two #42 #-99)); #print #"|";
	#print (#int_to_text (unused_effect #123)); #print #"|";
};
