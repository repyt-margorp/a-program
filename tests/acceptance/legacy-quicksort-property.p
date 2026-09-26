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
import partitionLower;
import partitionUpper;
import partitionByDecision;
import natAccessible;

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

appendResult := \A:@ => \xs:List A => xs @(self => (ys:List A)->AppendOf A self ys (append A self ys))
	@nil => (\ys:List A => (AppendOf A).nil ys)
	@cons h t => (\ys:List A => (AppendOf A).cons h t ys (append A t ys) (*t ys));
appendResult :: (A:@)->(xs:List A)->(ys:List A)->AppendOf A xs ys (append A xs ys);
measurementStep := \A:@ => \h:A => \t:List A => \out:Measured A => out
	@(self => MeasurementOf A t self->MeasurementOf A ((List A).cons h t)
		(self @measured n values => (Measured A).measured (Nat.succ n) ((SizedList A).cons n h values)))
	@measured n values => (\prior:MeasurementOf A t ((Measured A).measured n values) =>
		(MeasurementOf A).cons h t n values prior);
measurementResult := \A:@ => \xs:List A => xs @(self => MeasurementOf A self (measure A self))
	@nil => (MeasurementOf A).nil
	@cons h t => measurementStep A h t (measure A t) *t;
measurementResult :: (A:@)->(xs:List A)->MeasurementOf A xs (measure A xs);
partitionLowerResult := \A:@ => \h:A => \n:Nat => \t:SizedList A n => \parts:Partition A n => parts
	@(self => PartitionOf A n t self->PartitionOf A (Nat.succ n) ((SizedList A).cons n h t) (partitionLower A h n self))
	@parts l left u right lb ub => (\prior:PartitionOf A n t ((Partition A n).parts l left u right lb ub) =>
		(PartitionOf A).lower n h t l left u right lb ub prior
			(LT.lift l (Nat.succ n) lb) (LT.weakenRight u (Nat.succ n) ub));
partitionUpperResult := \A:@ => \h:A => \n:Nat => \t:SizedList A n => \parts:Partition A n => parts
	@(self => PartitionOf A n t self->PartitionOf A (Nat.succ n) ((SizedList A).cons n h t) (partitionUpper A h n self))
	@parts l left u right lb ub => (\prior:PartitionOf A n t ((Partition A n).parts l left u right lb ub) =>
		(PartitionOf A).upper n h t l left u right lb ub prior
			(LT.weakenRight l (Nat.succ n) lb) (LT.lift u (Nat.succ n) ub));
partitionDecisionResult := \A:@ => \h:A => \n:Nat => \t:SizedList A n => \answer:Bool =>
	\parts:Partition A n => \prior:PartitionOf A n t parts => answer
	@(self => PartitionOf A (Nat.succ n) ((SizedList A).cons n h t) (partitionByDecision A h n self parts))
	@true => partitionLowerResult A h n t parts prior
	@false => partitionUpperResult A h n t parts prior;
partitionResult := \A:@ => \le:A->A->Bool => \pivot:A => \n:Nat => \xs:SizedList A n => xs
	@(size self => PartitionOf A size self (partition A &le pivot size self))
	@nil => (PartitionOf A).nil (LT.step Nat.zero) (LT.step Nat.zero)
	@cons k h t => partitionDecisionResult A h k t (le h pivot) (partition A &le pivot k t) *t;
partitionResult :: (A:@)->(le:A->A->Bool)->(pivot:A)->(n:Nat)->(xs:SizedList A n)->
	PartitionOf A n xs (partition A &le pivot n xs);
joinResult := \A:@ => \le:A->A->Bool => \n:Nat => \pivot:A =>
	\down:(m:Nat)->LT m (Nat.succ n)->Acc Nat LT m => \parts:Partition A n => parts
	@parts l left u right lb ub => append A (quickSortAcc A &le l (down l lb) left)
		((List A).cons pivot (quickSortAcc A &le u (down u ub) right));
joinRearranged := \A:@ => \le:A->A->Bool => \n:Nat => \pivot:A => \tail:SizedList A n =>
	\down:(m:Nat)->LT m (Nat.succ n)->Acc Nat LT m =>
	\ih:(m:Nat)->(bound:LT m (Nat.succ n))->(xs:SizedList A m)->Rearranged A m xs (quickSortAcc A &le m (down m bound) xs) =>
	\parts:Partition A n => parts
	@(self => PartitionOf A n tail self->Rearranged A (Nat.succ n) ((SizedList A).cons n pivot tail) (joinResult A &le n pivot &down self))
	@parts l left u right lb ub => (\prior:PartitionOf A n tail ((Partition A n).parts l left u right lb ub) =>
		(Rearranged A).split n pivot tail l left u right lb ub prior
			(quickSortAcc A &le l (down l lb) left) (quickSortAcc A &le u (down u ub) right)
			(append A (quickSortAcc A &le l (down l lb) left) ((List A).cons pivot (quickSortAcc A &le u (down u ub) right)))
			(ih l lb left) (ih u ub right)
			(appendResult A (quickSortAcc A &le l (down l lb) left) ((List A).cons pivot (quickSortAcc A &le u (down u ub) right))));
rearrangedStep := \A:@ => \le:A->A->Bool => \n:Nat => \xs:SizedList A n => xs
	@(size self => (down:(m:Nat)->LT m size->Acc Nat LT m)->
		((m:Nat)->(bound:LT m size)->(values:SizedList A m)->Rearranged A m values (quickSortAcc A &le m (down m bound) values))->
		Rearranged A size self (quickSortAcc A &le size ((Acc Nat LT).acc size &down) self))
	@nil => (\down:(m:Nat)->LT m Nat.zero->Acc Nat LT m =>
		\ih:(m:Nat)->(bound:LT m Nat.zero)->(values:SizedList A m)->Rearranged A m values (quickSortAcc A &le m (down m bound) values) =>
		(Rearranged A).nil)
	@cons k h t => (\down:(m:Nat)->LT m (Nat.succ k)->Acc Nat LT m =>
		\ih:(m:Nat)->(bound:LT m (Nat.succ k))->(values:SizedList A m)->Rearranged A m values (quickSortAcc A &le m (down m bound) values) =>
		joinRearranged A &le k h t &down &ih (partition A &le h k t) (partitionResult A &le h k t));
rearrangedResult := \A:@ => \le:A->A->Bool => \n:Nat => \access:Acc Nat LT n => access
	@(size self => (xs:SizedList A size)->Rearranged A size xs (quickSortAcc A &le size self xs))
	@acc size down => (\xs:SizedList A size => rearrangedStep A &le size xs &down &*down);
rearrangedResult :: (A:@)->(le:A->A->Bool)->(n:Nat)->(access:Acc Nat LT n)->(xs:SizedList A n)->
	Rearranged A n xs (quickSortAcc A &le n access xs);
contentsMeasured := \A:@ => \le:A->A->Bool => \xs:List A => \out:Measured A => out
	@(self => MeasurementOf A xs self->ContentsOf A xs (self @measured n values => quickSortAcc A &le n (natAccessible n) values))
	@measured n values => (\measurement:MeasurementOf A xs ((Measured A).measured n values) =>
		(ContentsOf A).contents xs (quickSortAcc A &le n (natAccessible n) values) n values measurement
			(rearrangedResult A &le n (natAccessible n) values));
contentsResult := \A:@ => \le:A->A->Bool => \xs:List A =>
	contentsMeasured A &le xs (measure A xs) (measurementResult A xs);
contentsResult :: (A:@)->(le:A->A->Bool)->(xs:List A)->ContentsOf A xs (quickSort A &le xs);
certified := \le:Nat->Nat->Bool => \xs:List Nat =>
	readContents Nat xs (quickSort Nat &le xs) (contentsResult Nat &le xs);

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
