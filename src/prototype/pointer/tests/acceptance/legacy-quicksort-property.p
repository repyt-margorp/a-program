import Nat;
import Bool;
import List;
import SizedList;
import Measured;
import Partition;
import Acc;
import LT;
import partition;
import append;
import measure;
import quickSortAcc;
import quickSort;
import lessOrEqual;

AppendOf := \A:@ => @\left:List A => @\right:List A => @\output:List A => {
	nil : (right:List A) -> * (List A).nil right right;
	cons : (head:A) -> (tail:List A) -> (right:List A) -> (output:List A) ->
		* tail right output -> * ((List A).cons head tail) right ((List A).cons head output);
};
appendCorrect := \A:@ => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@nil following => (AppendOf A).nil following
	@cons head tail following tailOutput tailGraph =>
		(AppendOf A).cons head tail following tailOutput *tailGraph;
appendCorrect :: (A:@)->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->AppendOf A xs ys zs;

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

PartitionOf := \A:@ => @\size:Nat => @\input:SizedList A size => @\output:Partition A size => {
	nil : (lb:LT Nat.zero (Nat.succ Nat.zero)) -> (ub:LT Nat.zero (Nat.succ Nat.zero)) ->
		* Nat.zero (SizedList A).nil
			((Partition A Nat.zero).parts Nat.zero (SizedList A).nil Nat.zero (SizedList A).nil lb ub);
	lower : (n:Nat) -> (head:A) -> (tail:SizedList A n) ->
		(l:Nat) -> (left:SizedList A l) -> (u:Nat) -> (right:SizedList A u) ->
		(lb:LT l (Nat.succ n)) -> (ub:LT u (Nat.succ n)) ->
		* n tail ((Partition A n).parts l left u right lb ub) ->
		(nextLower:LT (Nat.succ l) (Nat.succ (Nat.succ n))) ->
		(nextUpper:LT u (Nat.succ (Nat.succ n))) ->
		* (Nat.succ n) ((SizedList A).cons n head tail)
			((Partition A (Nat.succ n)).parts (Nat.succ l) ((SizedList A).cons l head left)
				u right nextLower nextUpper);
	upper : (n:Nat) -> (head:A) -> (tail:SizedList A n) ->
		(l:Nat) -> (left:SizedList A l) -> (u:Nat) -> (right:SizedList A u) ->
		(lb:LT l (Nat.succ n)) -> (ub:LT u (Nat.succ n)) ->
		* n tail ((Partition A n).parts l left u right lb ub) ->
		(nextLower:LT l (Nat.succ (Nat.succ n))) ->
		(nextUpper:LT (Nat.succ u) (Nat.succ (Nat.succ n))) ->
		* (Nat.succ n) ((SizedList A).cons n head tail)
			((Partition A (Nat.succ n)).parts l left (Nat.succ u) ((SizedList A).cons u head right)
				nextLower nextUpper);
};

partitionCorrect := \A:@ => \le:A->A->Bool => \pivot:A => \n:Nat => \input:SizedList A n =>
	\output:Partition A n => \trace:@partition A &le pivot n input output => trace
	@nil => (PartitionOf A).nil (LT.step Nat.zero) (LT.step Nat.zero)
	@true size head tail decisionGraph l left u right lowerBound upperBound tailGraph =>
		(PartitionOf A).lower size head tail l left u right lowerBound upperBound *tailGraph
			(LT.lift l (Nat.succ size) lowerBound) (LT.weakenRight u (Nat.succ size) upperBound)
	@false size head tail decisionGraph l left u right lowerBound upperBound tailGraph =>
		(PartitionOf A).upper size head tail l left u right lowerBound upperBound *tailGraph
			(LT.weakenRight l (Nat.succ size) lowerBound) (LT.lift u (Nat.succ size) upperBound);
partitionCorrect :: (A:@) -> (le:A->A->Bool) -> (pivot:A) -> (n:Nat) -> (input:SizedList A n) ->
	(output:Partition A n) -> @partition A &le pivot n input output -> PartitionOf A n input output;

Rearranged := \A:@ => @\size:Nat => @\input:SizedList A size => @\output:List A => {
	nil : * Nat.zero (SizedList A).nil (List A).nil;
	split : (n:Nat) -> (pivot:A) -> (tail:SizedList A n) ->
		(l:Nat) -> (lower:SizedList A l) -> (u:Nat) -> (upper:SizedList A u) ->
		(lb:LT l (Nat.succ n)) -> (ub:LT u (Nat.succ n)) ->
		PartitionOf A n tail ((Partition A n).parts l lower u upper lb ub) ->
		(left:List A) -> (right:List A) -> (output:List A) ->
		* l lower left -> * u upper right -> AppendOf A left ((List A).cons pivot right) output ->
		* (Nat.succ n) ((SizedList A).cons n pivot tail) output;
};

sortCorrect := \A:@ => \le:A->A->Bool => \n:Nat => \access:Acc Nat LT n =>
	\input:SizedList A n => \output:List A => \trace:@quickSortAcc A &le n access input output => trace
	@nil down => (Rearranged A).nil
	@cons tailSize pivot tail down l lower u upper lb ub partitioning left leftGraph right rightGraph result appending =>
		(Rearranged A).split tailSize pivot tail l lower u upper lb ub
			(partitionCorrect A &le pivot tailSize tail ((Partition A tailSize).parts l lower u upper lb ub) partitioning)
			left right result *leftGraph *rightGraph (appendCorrect A left ((List A).cons pivot right) result appending);
sortCorrect :: (A:@) -> (le:A->A->Bool) -> (n:Nat) -> (access:Acc Nat LT n) ->
	(input:SizedList A n) -> (output:List A) -> @quickSortAcc A &le n access input output -> Rearranged A n input output;

ContentsOf := \A:@ => @\input:List A => @\output:List A => {
	contents : (input:List A) -> (output:List A) -> (n:Nat) -> (values:SizedList A n) ->
		MeasurementOf A input ((Measured A).measured n values) -> Rearranged A n values output -> * input output;
};
quickSortCorrect := \A:@ => \le:A->A->Bool => \xs:List A => \output:List A =>
	\trace:@quickSort A &le xs output => trace
	@measured original size values measurement access accessibility sorted sorting =>
		(ContentsOf A).contents original sorted size values
			(measureCorrect A original ((Measured A).measured size values) measurement)
			(sortCorrect A &le size access values sorted sorting);
quickSortCorrect :: (A:@) -> (le:A->A->Bool) -> (xs:List A) -> (output:List A) ->
	@quickSort A &le xs output -> ContentsOf A xs output;

readRearranged := \A:@ => \n:Nat => \input:SizedList A n => \output:List A => \proof:Rearranged A n input output => proof
	@nil => (List A).nil
	@split size pivot tail l lower u upper lb ub partitioning left right result lowerProof upperProof appending =>
		append A *lowerProof ((List A).cons pivot *upperProof);
readContents := \A:@ => \input:List A => \output:List A => \proof:ContentsOf A input output => proof
	@contents original result size values measurement rearrangement =>
		readRearranged A size values result rearrangement;
certified := \le:Nat->Nat->Bool => \xs:List Nat => *quickSort Nat &le xs @output =>
	readContents Nat xs output (quickSortCorrect Nat &le xs output @output);

one := Nat.succ Nat.zero;
two := Nat.succ one;
three := Nat.succ two;
empty := (List Nat).nil;
singleton := (List Nat).cons two empty;
ascending := (List Nat).cons one ((List Nat).cons two ((List Nat).cons three empty));
descending := (List Nat).cons three ((List Nat).cons two ((List Nat).cons one empty));
mixed := (List Nat).cons two ((List Nat).cons three ((List Nat).cons one empty));
duplicates := (List Nat).cons two ((List Nat).cons one ((List Nat).cons two empty));
duplicatesExpected := (List Nat).cons one ((List Nat).cons two ((List Nat).cons two empty));
main := certified &lessOrEqual mixed;
emptyMain := certified &lessOrEqual empty;
singletonMain := certified &lessOrEqual singleton;
ascendingMain := certified &lessOrEqual ascending;
descendingMain := certified &lessOrEqual descending;
duplicatesMain := certified &lessOrEqual duplicates;
alwaysFalse := \x:Nat => \y:Nat => Bool.false;
unorderedMain := certified &alwaysFalse mixed;
