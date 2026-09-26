import Nat;
import List;
import natLessOrEqual;
import quickSort;
adequacy := \xs:List Nat => *quickSort Nat (&natLessOrEqual) xs @output => @output;
adequacy :: (xs:List Nat) ->
	@quickSort Nat (&natLessOrEqual) xs (quickSort Nat (&natLessOrEqual) xs);
