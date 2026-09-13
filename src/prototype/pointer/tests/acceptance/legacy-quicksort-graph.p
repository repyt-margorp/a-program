import Nat;
import List;
import SizedList;
import Measured;
import Acc;
import LT;
import Bool;
import lessOrEqual;
import measure;
import natAccessible;
import quickSortAcc;
import quickSort;

one := Nat.succ Nat.zero;
two := Nat.succ one;
input := (List Nat).cons two ((List Nat).cons one (List Nat).nil);
expected := (List Nat).cons one ((List Nat).cons two (List Nat).nil);
empty := (List Nat).nil;
main := *quickSort Nat &lessOrEqual input @output => output;
emptyMain := *quickSort Nat &lessOrEqual empty @output => output;

readGraph := \A:@ => \le:A->A->Bool => \xs:List A => \output:List A => \trace:@quickSort A &le xs output => trace
	@measured original size values measurement access accessibility sorted sorting => {
		measurement :: @measure A original ((Measured A).measured size values);
		access :: Acc Nat LT size;
		accessibility :: @natAccessible size access;
		sorting :: @quickSortAcc A &le size access values sorted;
		sorted;
	};
readGraph :: (A:@) -> (le:A->A->Bool) -> (xs:List A) -> (output:List A) ->
	@quickSort A &le xs output -> List A;
graphMain := *quickSort Nat &lessOrEqual input @output =>
	readGraph Nat &lessOrEqual input output @output;
