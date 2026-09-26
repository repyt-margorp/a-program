// Bounds and graph induction for the exact PR #30 merge-sort program.
import Nat;
import List;
import Halves;
import Measured;
import SizedList;
import Sorted;
import natLessOrEqual;
import mergeBy;
import mergeSortFuel;
import mergeSort;
import splitAlternating;
import measure;
import generic_sorted;
Fits := @\fuel:Nat => @\xs:List Nat => {
	nil:(n:Nat)->* n (List Nat).nil;
	cons:(n:Nat)->(h:Nat)->(t:List Nat)->* n t->* (Nat.succ n) ((List Nat).cons h t);
};
fits_weaken := \n:Nat => \xs:List Nat => \p:Fits n xs => p
	@nil k => Fits.nil (Nat.succ k)
	@cons k h t prior => Fits.cons (Nat.succ k) h t *prior;
fits_weaken :: (n:Nat)->(xs:List Nat)->Fits n xs->Fits (Nat.succ n) xs;
fits_tail := \n:Nat => \h:Nat => \t:List Nat => \p:Fits (Nat.succ n) ((List Nat).cons h t) => p
	@cons k a b prior => prior;
fits_tail :: (n:Nat)->(h:Nat)->(t:List Nat)->Fits (Nat.succ n) ((List Nat).cons h t)->Fits n t;
zero_sorted := \xs:List Nat => \p:Fits Nat.zero xs => p @nil n => Sorted.nil;
zero_sorted :: (xs:List Nat)->Fits Nat.zero xs->Sorted xs;
merge_ordered := \xs:List Nat => \ys:List Nat => \zs:List Nat =>
	\g:@mergeBy Nat (&natLessOrEqual) xs ys zs => g
	@case0 => (\p:Sorted ys => p)
	@case1 h t tail rest output inserted => (\p:Sorted ys => generic_sorted h tail output inserted (*rest p));
merge_ordered :: (xs:List Nat)->(ys:List Nat)->(zs:List Nat)->
	@mergeBy Nat (&natLessOrEqual) xs ys zs->Sorted ys->Sorted zs;
FitsPair := \n:Nat => @\parts:Halves Nat => {
	halves:(l:List Nat)->(r:List Nat)->Fits n l->Fits n r->* ((Halves Nat).halves l r);
};
pair_left := \n:Nat => \l:List Nat => \r:List Nat => \p:FitsPair n ((Halves Nat).halves l r) => p
	@halves a b left right => left;
pair_right := \n:Nat => \l:List Nat => \r:List Nat => \p:FitsPair n ((Halves Nat).halves l r) => p
	@halves a b left right => right;
pair_step := \h:Nat => \n:Nat => \l:List Nat => \r:List Nat => \p:FitsPair n ((Halves Nat).halves l r) =>
	(FitsPair (Nat.succ n)).halves ((List Nat).cons h r) l
		(Fits.cons n h r (pair_right n l r p)) (fits_weaken n l (pair_left n l r p));
split_fits_step := \h:Nat => \t:List Nat => \l:List Nat => \r:List Nat =>
	\ih:(n:Nat)->Fits n t->FitsPair n ((Halves Nat).halves l r) =>
	\n:Nat => \p:Fits n ((List Nat).cons h t) => p
	@cons k a b prior => pair_step a k l r (ih k prior);
split_fits := \xs:List Nat => \parts:Halves Nat => \g:@splitAlternating Nat xs parts => g
	@case0 => (\n:Nat => \p:Fits n (List Nat).nil =>
		(FitsPair n).halves (List Nat).nil (List Nat).nil (Fits.nil n) (Fits.nil n))
	@case1 h t l r rest => split_fits_step h t l r &*rest;
split_fits :: (xs:List Nat)->(parts:Halves Nat)->@splitAlternating Nat xs parts->(n:Nat)->Fits n xs->FitsPair n parts;
right_bounded := \n:Nat => \h:Nat => \t:List Nat => \l:List Nat => \r:List Nat =>
	\g:@splitAlternating Nat ((List Nat).cons h t) ((Halves Nat).halves l r) =>
	\p:Fits (Nat.succ n) ((List Nat).cons h t) => g
	@case1 a b left right rest => pair_left n left right
		(split_fits b ((Halves Nat).halves left right) rest n (fits_tail n a b p));
right_bounded :: (n:Nat)->(h:Nat)->(t:List Nat)->(l:List Nat)->(r:List Nat)->
	@splitAlternating Nat ((List Nat).cons h t) ((Halves Nat).halves l r)->Fits (Nat.succ n) ((List Nat).cons h t)->Fits n r;
MeasurementFits := \xs:List Nat => @\out:Measured Nat => {
	measured:(n:Nat)->(values:SizedList Nat n)->Fits n xs->* ((Measured Nat).measured n values);
};
measurement_bound := \xs:List Nat => \n:Nat => \values:SizedList Nat n =>
	\p:MeasurementFits xs ((Measured Nat).measured n values) => p @measured k v bound => bound;
measure_fits := \xs:List Nat => \out:Measured Nat => \g:@measure Nat xs out => g
	@case0 => (MeasurementFits (List Nat).nil).measured Nat.zero (SizedList Nat).nil (Fits.nil Nat.zero)
	@case1 h t n values rest => (MeasurementFits ((List Nat).cons h t)).measured
		(Nat.succ n) ((SizedList Nat).cons n h values) (Fits.cons n h t (measurement_bound t n values *rest));
measure_fits :: (xs:List Nat)->(out:Measured Nat)->@measure Nat xs out->MeasurementFits xs out;
fuel_correct := \fuel:Nat => \xs:List Nat => \ys:List Nat =>
	\g:@mergeSortFuel (&natLessOrEqual) fuel xs ys => g
	@case0 input => (\bound:Fits Nat.zero input => zero_sorted input bound)
	@case1 remaining => (\bound:Fits (Nat.succ remaining) (List Nat).nil => Sorted.nil)
	@case2 remaining h t l r split left lg right rg output merging =>
		(\bound:Fits (Nat.succ remaining) ((List Nat).cons h t) =>
			merge_ordered left right output merging
				(*rg (right_bounded remaining h t l r split bound)));
fuel_correct :: (fuel:Nat)->(xs:List Nat)->(ys:List Nat)->
	@mergeSortFuel (&natLessOrEqual) fuel xs ys->Fits fuel xs->Sorted ys;
merge_correct := \xs:List Nat => \ys:List Nat => \g:@mergeSort (&natLessOrEqual) xs ys => g
	@case0 original size values measurement output sorting =>
		fuel_correct size original output sorting
			(measurement_bound original size values
				(measure_fits original ((Measured Nat).measured size values) measurement));
merge_correct :: (xs:List Nat)->(ys:List Nat)->@mergeSort (&natLessOrEqual) xs ys->Sorted ys;
import insert_result_sorted;
merge_ordered_result := \xs:List Nat => \ys:List Nat => xs
	@(self => Sorted ys->Sorted (mergeBy Nat (&natLessOrEqual) self ys))
	@nil => (\p:Sorted ys => p)
	@cons h t => (\p:Sorted ys => insert_result_sorted h (mergeBy Nat (&natLessOrEqual) t ys) (*t p));
merge_ordered_result :: (xs:List Nat)->(ys:List Nat)->Sorted ys->Sorted (mergeBy Nat (&natLessOrEqual) xs ys);
split_graph_step := \h:Nat => \t:List Nat => \parts:Halves Nat => parts
	@(self => @splitAlternating Nat t self->@splitAlternating Nat ((List Nat).cons h t)
		(self @halves l r => (Halves Nat).halves ((List Nat).cons h r) l))
	@halves l r => (\rest:@splitAlternating Nat t ((Halves Nat).halves l r) =>
		(@splitAlternating Nat).case1 h t l r rest);
split_graph := \xs:List Nat => xs @(self => @splitAlternating Nat self (splitAlternating Nat self))
	@nil => (@splitAlternating Nat).case0
	@cons h t => split_graph_step h t (splitAlternating Nat t) *t;
split_graph :: (xs:List Nat)->@splitAlternating Nat xs (splitAlternating Nat xs);
fuel_result_split := \n:Nat => \h:Nat => \t:List Nat => \bound:Fits (Nat.succ n) ((List Nat).cons h t) =>
	\ih:(xs:List Nat)->Fits n xs->Sorted (mergeSortFuel (&natLessOrEqual) n xs) => \parts:Halves Nat => parts
	@(self => @splitAlternating Nat ((List Nat).cons h t) self->Sorted
		(self @halves l r => mergeBy Nat (&natLessOrEqual)
			(mergeSortFuel (&natLessOrEqual) n l) (mergeSortFuel (&natLessOrEqual) n r)))
	@halves l r => (\graph:@splitAlternating Nat ((List Nat).cons h t) ((Halves Nat).halves l r) =>
		merge_ordered_result (mergeSortFuel (&natLessOrEqual) n l) (mergeSortFuel (&natLessOrEqual) n r)
			(ih r (right_bounded n h t l r graph bound)));
fuel_result_sorted := \fuel:Nat => fuel
	@(n => (xs:List Nat)->Fits n xs->Sorted (mergeSortFuel (&natLessOrEqual) n xs))
	@zero => (\xs:List Nat => zero_sorted xs)
	@succ n => (\xs:List Nat => xs
		@(self => Fits (Nat.succ n) self->Sorted (mergeSortFuel (&natLessOrEqual) (Nat.succ n) self))
		@nil => (\bound:Fits (Nat.succ n) (List Nat).nil => Sorted.nil)
		@cons h t => (\bound:Fits (Nat.succ n) ((List Nat).cons h t) =>
			fuel_result_split n h t bound &*n (splitAlternating Nat ((List Nat).cons h t))
				(split_graph ((List Nat).cons h t))));
fuel_result_sorted :: (n:Nat)->(xs:List Nat)->Fits n xs->Sorted (mergeSortFuel (&natLessOrEqual) n xs);
measure_graph_step := \h:Nat => \t:List Nat => \out:Measured Nat => out
	@(self => @measure Nat t self->@measure Nat ((List Nat).cons h t)
		(self @measured n values => (Measured Nat).measured (Nat.succ n) ((SizedList Nat).cons n h values)))
	@measured n values => (\rest:@measure Nat t ((Measured Nat).measured n values) => (@measure Nat).case1 h t n values rest);
measure_graph := \xs:List Nat => xs @(self => @measure Nat self (measure Nat self))
	@nil => (@measure Nat).case0
	@cons h t => measure_graph_step h t (measure Nat t) *t;
measure_graph :: (xs:List Nat)->@measure Nat xs (measure Nat xs);
merge_result_measured := \xs:List Nat => \out:Measured Nat => out
	@(self => MeasurementFits xs self->Sorted (self @measured n values => mergeSortFuel (&natLessOrEqual) n xs))
	@measured n values => (\p:MeasurementFits xs ((Measured Nat).measured n values) =>
		fuel_result_sorted n xs (measurement_bound xs n values p));
merge_result_sorted := \xs:List Nat => merge_result_measured xs (measure Nat xs)
	(measure_fits xs (measure Nat xs) (measure_graph xs));
merge_result_sorted :: (xs:List Nat)->Sorted (mergeSort (&natLessOrEqual) xs);
import read_sorted;
import one;
import two;
import three;
four := Nat.succ three;
sample := (List Nat).cons two ((List Nat).cons Nat.zero ((List Nat).cons one ((List Nat).cons one (List Nat).nil)));
expected_value := (List Nat).cons Nat.zero ((List Nat).cons one ((List Nat).cons one ((List Nat).cons two (List Nat).nil)));
main := read_sorted (mergeSort (&natLessOrEqual) sample) (merge_result_sorted sample);
empty := read_sorted (mergeSort (&natLessOrEqual) (List Nat).nil) (merge_result_sorted (List Nat).nil);
singleton := read_sorted (mergeSort (&natLessOrEqual) ((List Nat).cons one (List Nat).nil))
	(merge_result_sorted ((List Nat).cons one (List Nat).nil));
already := read_sorted (mergeSort (&natLessOrEqual) expected_value) (merge_result_sorted expected_value);
reversed_value := (List Nat).cons two ((List Nat).cons one ((List Nat).cons one ((List Nat).cons Nat.zero (List Nat).nil)));
reversed := read_sorted (mergeSort (&natLessOrEqual) reversed_value) (merge_result_sorted reversed_value);
packet_value := mergeSort (&natLessOrEqual) sample;
direct_value := mergeSort (&natLessOrEqual) sample;
zero := Nat.zero;
one_value := Nat.succ zero;
