import Nat;
import Bool;
import List;
import SizedList;
import Partition;
import LT;
import partition;
import lessOrEqual;

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

readPartition := \A:@ => \n:Nat => \input:SizedList A n => \output:Partition A n =>
	\proof:PartitionOf A n input output => proof
	@nil lb ub => (List A).nil
	@lower size head tail l left u right lb ub rest nextLower nextUpper => (List A).cons head *rest
	@upper size head tail l left u right lb ub rest nextLower nextUpper => (List A).cons head *rest;

import partitionLower;
import partitionUpper;
import partitionByDecision;
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
checkPartition := \pivot:Nat => \n:Nat => \input:SizedList Nat n =>
	readPartition Nat n input (partition Nat &lessOrEqual pivot n input) (partitionResult Nat &lessOrEqual pivot n input);
one := Nat.succ Nat.zero;
two := Nat.succ one;
three := Nat.succ two;
empty := (List Nat).nil;
input := (SizedList Nat).cons two two
	((SizedList Nat).cons one Nat.zero ((SizedList Nat).cons Nat.zero one (SizedList Nat).nil));
expected := (List Nat).cons two ((List Nat).cons Nat.zero ((List Nat).cons one empty));
main := checkPartition one three input;
lowerMain := checkPartition three three input;
upperMain := checkPartition Nat.zero two
	((SizedList Nat).cons one two ((SizedList Nat).cons Nat.zero one (SizedList Nat).nil));
upperExpected := (List Nat).cons two ((List Nat).cons one empty);
duplicatesMain := checkPartition one two
	((SizedList Nat).cons one one ((SizedList Nat).cons Nat.zero one (SizedList Nat).nil));
duplicatesExpected := (List Nat).cons one ((List Nat).cons one empty);
emptyMain := checkPartition one Nat.zero (SizedList Nat).nil;
