import structuralMerge;
import Bool;
import Nat;
import List;
import lessEqual;
import left;
import right;
import nil;
import expected;

graph := @structuralMerge;
main := structuralMerge Nat &lessEqual left right;
graphExpected := expected;
emptyMain := structuralMerge Nat &lessEqual nil right;
emptyExpected := right;

// Isolate captured helper specialization from nested comparison recursion.
copyLeft := \xs : List Nat => \ys : List Nat => xs
	@nil => ys
	@cons head tail => (List Nat).cons head *tail;
repeatCopy := \fuel : Nat => fuel
	@zero => (\xs : List Nat => xs)
	@succ k => (\xs : List Nat => {
		previous := *k xs;
		copyLeft left previous;
	});
repeatGraph := @repeatCopy;
repeatMain := repeatCopy (Nat.succ Nat.zero) right;
repeatExpected := copyLeft left right;

// A non-recursive Match with a captured later argument uses the same path.
choose := \b : Bool => \x : Nat => b
	@true => x
	@false => Nat.zero;
chooseGraph := @choose;
chosen := choose Bool.true (Nat.succ Nat.zero);
one := Nat.succ Nat.zero;

// A captured eliminator may itself return a function from each branch.
chooseLater := \b : Bool => \x : Nat => b
	@true => (\y : Nat => x)
	@false => (\y : Nat => y);
laterGraph := @chooseLater;
laterMain := chooseLater Bool.false Nat.zero one;

// Moving n behind v would invalidate v's domain. Keep the environment intact.
pick := \P : Nat -> @ => \n : Nat => \v : P n => n
	@zero => v
	@succ k => *k;
pickGraph := @pick;
Family := \n : Nat => n @zero => Nat @succ k => Bool;
dependentMain := pick &Family one Bool.true;
dependentExpected := Bool.true;

// Preserve helper calls exposed through partial application and sequencing.
repeatMerge := \le : Nat -> Nat -> Bool => \fuel : Nat => fuel
	@zero => (\xs : List Nat => xs)
	@succ k => (\xs : List Nat => {
		previous := *k xs;
		structuralMerge Nat le left previous;
	});
mergeGraph := @repeatMerge;
mergedMain := repeatMerge &lessEqual one right;
mergedZero := repeatMerge &lessEqual Nat.zero right;
mergedTwice := repeatMerge &lessEqual (Nat.succ one) right;
mergedTwiceExpected := structuralMerge Nat &lessEqual left expected;
