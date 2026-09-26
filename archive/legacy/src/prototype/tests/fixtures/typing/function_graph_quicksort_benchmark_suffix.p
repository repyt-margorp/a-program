leftIsZero := \left : Nat => \right : Nat =>
	left @zero => Bool.true @succ predecessor => Bool.false;

partitionGraphProbe := \pivot : Nat => \size : Nat =>
	\input : SizedList Nat size =>
	*partition Nat &leftIsZero pivot size input;

quickSortGraphProbe := *quickSort Nat &leftIsZero sample;

inspectQuickSortAcc := \LeGraph : (left : Nat) -> (right : Nat) -> Bool -> @ =>
	\size : Nat => \access : Acc Nat LT size =>
	\input : SizedList Nat size => \output : List Nat =>
	\graph : @quickSortAcc Nat LeGraph size access input output =>
	graph
	@nil current down => output
	@cons tailSize pivot tail current down lowerSize lower upperSize upper
		lowerBound upperBound lowerOutput lowerGraph upperOutput upperGraph
		result appendGraph => output;
