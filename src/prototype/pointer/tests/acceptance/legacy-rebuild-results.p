import Nat;
import LE;
import OrderedFrom;
import rebuild;

one := Nat.succ Nat.zero;
emptyMain := rebuild one (OrderedFrom.nil one);
emptyMainExpected := OrderedFrom.nil one;

input := OrderedFrom.cons Nat.zero one (LE.zero one)
	(OrderedFrom.cons one one (LE.succ Nat.zero Nat.zero (LE.zero Nat.zero))
		(OrderedFrom.nil one));
recursiveMain := rebuild Nat.zero input;
recursiveMainExpected := input;
