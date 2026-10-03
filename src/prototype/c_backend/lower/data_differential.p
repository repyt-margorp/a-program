import Bool;
import Packet;
import number;
import rebuild;
import captured;
import partial;
import shadowed;

report := \p : Packet => {
	#print (#int_to_text (number p)); #print #" ";
	#print (#int_to_text (number (rebuild p))); #print #" ";
	#print (#int_to_text (captured p Bool.false)); #print #" ";
	#print (#int_to_text (captured p Bool.true)); #print #" ";
	#print (#int_to_text (partial p #7)); #print #" ";
	#print (#int_to_text (shadowed p)); #print #"|";
};
both := \n : #Int32 => {
	report (Packet.small n Bool.false);
	report (Packet.small n Bool.true);
};
main := {
	report Packet.empty;
	both #-2147483648; both #-1; both #0; both #2147483647;
};
