import Bool;
import Packet;
import Envelope;
import Records;
import Reverse;
import length;
import sum;
import append;
import prepend;
import reverse_sum;
import reverse_prepend;
import sample;
report := \xs : Records => {
	#print (#int_to_text (length xs)); #print #" ";
	#print (#int_to_text (sum xs)); #print #" ";
	#print (#int_to_text (sum (append xs xs))); #print #" ";
	#print (#int_to_text (sum (prepend (Envelope.pair (Packet.small #4 Bool.true) #3) xs))); #print #"|";
};
main := {
	report Records.nil;
	report sample;
	report (Records.cons (Envelope.pair (Packet.small #2147483647 Bool.true) #0) Records.nil);
	#print (#int_to_text (reverse_sum (reverse_prepend (Envelope.pair Packet.empty #9) Reverse.end)));
};
