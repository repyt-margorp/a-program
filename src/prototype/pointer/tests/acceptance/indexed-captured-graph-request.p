import Nat;
import Vec;
import count;
import select;
import one;
import two;
import three;
import sample;

countGraph := @count;
countWitness := *count;
main := *count Nat two sample one @output => output;
emptyMain := *count Nat Nat.zero (Vec Nat).nil one @output => output;
expected := three;
emptyExpected := one;
selectGraph := @select;
selected := *select Nat two sample one @output => output;

// Keep the output's actual fiber when specializing the private graph.
copy := \A : @ => \n : Nat => \xs : Vec A n => \ignored : Nat => xs
	@nil => (Vec A).nil
	@cons k head tail => (Vec A).cons k head (*tail ignored);
copyGraph := @copy;
copyMain := *copy Nat two sample one @output => output;
copyEmpty := *copy Nat Nat.zero (Vec Nat).nil one @output => output;
copyExpected := sample;
empty := (Vec Nat).nil;

// A helper call must apply the same specialization to its result classifier.
repeatCount := \fuel : Nat => fuel
	@zero => (\start : Nat => start)
	@succ k => (\start : Nat => {
		previous := *k start;
		count Nat two sample previous;
	});
repeatGraph := @repeatCount;
repeated := *repeatCount two one @output => output;
five := Nat.succ (Nat.succ three);

// The second index's type depends on the first; preserve telescope order.
Point := @\A : @ => \x : A => {
	point : (B : @) -> (y : B) -> * B y;
};
pointSelect := \A : @ => \x : A => \p : Point A x => \result : Nat => p
	@point B y => result;
pointGraph := @pointSelect;
pointMain := *pointSelect Nat one (Point.point Nat one) one @output => output;

// Specialize captured arguments, but leave a branch-local raw Pi callable.
selectLater := \A : @ => \n : Nat => \xs : Vec A n => \start : Nat => xs
	@nil => (\later : Nat => start)
	@cons k head tail => (\later : Nat => later);
laterGraph := @selectLater;
laterMain := *selectLater Nat two sample one two @output => output;
laterEmpty := *selectLater Nat Nat.zero (Vec Nat).nil one two @output => output;
laterExpected := two;
