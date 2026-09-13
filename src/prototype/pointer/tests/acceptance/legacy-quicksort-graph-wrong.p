import Nat;
import List;
import Bool;
import quickSortAcc;
import quickSort;

wrong := \A:@ => \le:A->A->Bool => \xs:List A => \output:List A => \trace:@quickSort A &le xs output => trace
	@measured original size values measurement access accessibility sorted sorting =>
		(sorting :: @quickSortAcc A &le size access values (List A).nil);
