import Flag;
import Flags;
import Signal;
import Signals;
import length;
import count_on;
import flip;
import append;
import signal_length;
import signal_score;
import signal_append;
report_flags := \xs : Flags => {
	#print (#int_to_text (length xs)); #print #" ";
	#print (#int_to_text (count_on xs)); #print #" ";
	#print (#int_to_text (count_on (flip xs))); #print #" ";
	#print (#int_to_text (count_on (append xs xs))); #print #"|";
};
report_signals := \xs : Signals => {
	#print (#int_to_text (signal_length xs)); #print #" ";
	#print (#int_to_text (signal_score xs)); #print #" ";
	#print (#int_to_text (signal_score (signal_append xs xs))); #print #" ";
	#print (#int_to_text (signal_length (signal_append xs xs))); #print #"|";
};
main := {
	report_flags Flags.nil;
	report_flags (Flags.cons Flag.off (Flags.cons Flag.on (Flags.cons Flag.on Flags.nil)));
	report_flags (Flags.cons Flag.off (Flags.cons Flag.on (Flags.cons Flag.off (Flags.cons Flag.on Flags.nil))));
	report_flags (Flags.cons Flag.on (Flags.cons Flag.on (Flags.cons Flag.on Flags.nil)));
	report_signals Signals.end;
	report_signals (Signals.more (Signals.more (Signals.more Signals.end Signal.amber) Signal.green) Signal.red);
};
