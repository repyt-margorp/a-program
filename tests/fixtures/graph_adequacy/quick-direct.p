import Nat;
import Bool;
import List;
import SizedList;
import Partition;
import Acc;
import LT;
import partitionLower;
import partitionUpper;
import partitionByDecision;
import partition;
import quickSortAcc;
import quickSort;
import append;

append_complete := \A:@ => \xs:List A =>
	xs @(self => (ys:List A) -> @append A self ys (append A self ys))
	@nil => (\ys:List A => (@append A).case0 ys)
	@cons head tail => (\ys:List A =>
		(@append A).case1 head tail ys (append A tail ys) (*tail ys));
append_complete :: (A:@)->(xs:List A)->(ys:List A)->
	@append A xs ys (append A xs ys);

lower_complete := \A:@ => \head:A => \n:Nat => \parts:Partition A n =>
	parts @(self => @partitionLower A head n self (partitionLower A head n self))
	@parts l left r right lb rb => (@partitionLower A head n).case0 l left r right lb rb;
lower_complete :: (A:@)->(head:A)->(n:Nat)->(parts:Partition A n)->
	@partitionLower A head n parts (partitionLower A head n parts);

upper_complete := \A:@ => \head:A => \n:Nat => \parts:Partition A n =>
	parts @(self => @partitionUpper A head n self (partitionUpper A head n self))
	@parts l left r right lb rb => (@partitionUpper A head n).case0 l left r right lb rb;
upper_complete :: (A:@)->(head:A)->(n:Nat)->(parts:Partition A n)->
	@partitionUpper A head n parts (partitionUpper A head n parts);

decision_complete := \A:@ => \head:A => \n:Nat => \decision:Bool => \parts:Partition A n =>
	decision @(self => @partitionByDecision A head n self parts (partitionByDecision A head n self parts))
	@true => (@partitionByDecision A head n Bool.true parts (partitionLower A head n parts)).case0
		(partitionLower A head n parts) (lower_complete A head n parts)
	@false => (@partitionByDecision A head n Bool.false parts (partitionUpper A head n parts)).case1
		(partitionUpper A head n parts) (upper_complete A head n parts);
decision_complete :: (A:@)->(head:A)->(n:Nat)->(decision:Bool)->(parts:Partition A n)->
	@partitionByDecision A head n decision parts (partitionByDecision A head n decision parts);
