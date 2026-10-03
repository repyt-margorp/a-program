import Bool;
import Packet;
import Envelope;
import wrap;
import measure;
import rebuild;
import captured;
import outer;
import outer_number;
report := \e : Envelope => {
	#print (#int_to_text (measure e)); #print #" ";
	#print (#int_to_text (measure (rebuild e))); #print #" ";
	#print (#int_to_text (captured e)); #print #" ";
	#print (#int_to_text (outer_number (outer e))); #print #"|";
};
main := {
	report Envelope.none;
	report (wrap Packet.empty #3);
	report (wrap (Packet.small #4 Bool.true) #3);
	report (wrap (Packet.small #2147483647 Bool.true) #0);
};
