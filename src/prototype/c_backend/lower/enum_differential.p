import Bool;
import to_int;
import choose;
import nested;
import curried;
import through_fold;
import fold_arg;
import colour;
import to_colour;

report := \b : Bool => {
	#print (#int_to_text (to_int b)); #print #" ";
	#print (#int_to_text (choose b #7 #11)); #print #" ";
	#print (#int_to_text (nested b)); #print #" ";
	#print (#int_to_text (curried b #5)); #print #" ";
	#print (#int_to_text (through_fold b)); #print #" ";
	#print (#int_to_text (fold_arg b)); #print #" ";
	#print (#int_to_text (colour (to_colour b))); #print #"|";
};
main := { report Bool.false; report Bool.true; };
