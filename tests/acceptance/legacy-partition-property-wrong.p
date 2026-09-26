import Nat;
import Bool;
import SizedList;
import Partition;
import LT;
import partition;

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

// A proof about the tail cannot establish preservation of the whole input.
wrong := \A:@ => \le:A->A->Bool => \pivot:A => \n:Nat => \input:SizedList A n =>
	\output:Partition A n => \trace:@partition A &le pivot n input output => trace
	@nil => (PartitionOf A).nil (LT.step Nat.zero) (LT.step Nat.zero)
	@true size head tail decisionGraph l left u right lowerBound upperBound tailGraph => *tailGraph
	@false size head tail decisionGraph l left u right lowerBound upperBound tailGraph => *tailGraph;
wrong :: (A:@) -> (le:A->A->Bool) -> (pivot:A) -> (n:Nat) -> (input:SizedList A n) ->
	(output:Partition A n) -> @partition A &le pivot n input output -> PartitionOf A n input output;
