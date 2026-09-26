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
measure_graph_step := \A:@ => \h:A => \t:List A => \out:Measured A => out
	@(self => @measure A t self->@measure A ((List A).cons h t)
		(self @measured n values => (Measured A).measured (Nat.succ n) ((SizedList A).cons n h values)))
	@measured n values => (\rest:@measure A t ((Measured A).measured n values) => (@measure A).cons h t n values rest);
measure_graph := \A:@ => \xs:List A => xs @(self => @measure A self (measure A self))
	@nil => (@measure A).nil
	@cons h t => measure_graph_step A h t (measure A t) *t;
measure_graph :: (A:@)->(xs:List A)->@measure A xs (measure A xs);
one := Nat.succ Nat.zero;
input := (List Nat).cons one ((List Nat).cons Nat.zero (List Nat).nil);
main := readMeasurement Nat input (measure Nat input) (measureCorrect Nat input (measure Nat input) (measure_graph Nat input));
empty := (List Nat).nil;
emptyMain := readMeasurement Nat empty (measure Nat empty) (measureCorrect Nat empty (measure Nat empty) (measure_graph Nat empty));
