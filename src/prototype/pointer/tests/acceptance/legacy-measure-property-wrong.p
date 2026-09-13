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

wrong := \A:@ => \xs:List A => \output:Measured A => \trace:@measure A xs output => trace
	@nil => (MeasurementOf A).nil
	@cons head tail size values tailGraph => *tailGraph;
wrong :: (A:@) -> (xs:List A) -> (output:Measured A) ->
	@measure A xs output -> MeasurementOf A xs output;
