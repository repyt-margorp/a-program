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
witness := *structuralMerge;
main := *structuralMerge Nat &lessEqual left right @output => output;
graphExpected := expected;
emptyMain := *structuralMerge Nat &lessEqual nil right @output => output;
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
repeatWitness := *repeatCopy;
repeatMain := *repeatCopy (Nat.succ Nat.zero) right @output => output;
repeatExpected := copyLeft left right;

// A non-recursive Match with a captured later argument uses the same path.
choose := \b : Bool => \x : Nat => b
	@true => x
	@false => Nat.zero;
chooseGraph := @choose;
chooseWitness := *choose;
chosen := *choose Bool.true (Nat.succ Nat.zero) @output => output;
one := Nat.succ Nat.zero;

// A captured eliminator may itself return a function from each branch.
chooseLater := \b : Bool => \x : Nat => b
	@true => (\y : Nat => x)
	@false => (\y : Nat => y);
laterGraph := @chooseLater;
laterWitness := *chooseLater;
laterMain := *chooseLater Bool.false Nat.zero one @output => output;

// Moving n behind v would invalidate v's domain. Keep the environment intact.
pick := \P : Nat -> @ => \n : Nat => \v : P n => n
	@zero => v
	@succ k => *k;
pickGraph := @pick;
pickWitness := *pick;
Family := \n : Nat => n @zero => Nat @succ k => Bool;
dependentMain := *pick &Family one Bool.true @output => output;
dependentExpected := Bool.true;
