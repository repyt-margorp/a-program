import Nat;
import List;
import SizedList;
import Measured;
import measure;

MeasurementOf := \A:@ => @\input:List A => @\output:Measured A => {
	nil : * (List A).nil ((Measured A).measured Nat.zero (SizedList A).nil);
	cons : (head:A) -> (tail:List A) -> (size:Nat) -> (values:SizedList A size) ->
		* tail ((Measured A).measured size values) ->
		* ((List A).cons head tail)
			((Measured A).measured (Nat.succ size) ((SizedList A).cons size head values));
};

measureCorrect := \A:@ => \xs:List A => \output:Measured A => \trace:@measure A xs output => trace
	@nil => (MeasurementOf A).nil
	@cons head tail size values tailGraph =>
		(MeasurementOf A).cons head tail size values *tailGraph;
measureCorrect :: (A:@) -> (xs:List A) -> (output:Measured A) ->
	@measure A xs output -> MeasurementOf A xs output;

readMeasurement := \A:@ => \xs:List A => \output:Measured A => \proof:MeasurementOf A xs output => proof
	@nil => (List A).nil
	@cons head tail size values rest => (List A).cons head *rest;
one := Nat.succ Nat.zero;
input := (List Nat).cons one ((List Nat).cons Nat.zero (List Nat).nil);
main := *measure Nat input @output =>
	readMeasurement Nat input output (measureCorrect Nat input output @output);
empty := (List Nat).nil;
emptyMain := *measure Nat empty @output =>
	readMeasurement Nat empty output (measureCorrect Nat empty output @output);
