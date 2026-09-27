import Bool;
import List;
import general_decision;
import general_locally_sorted;
import quick_locally_sorted;
import quickSort;

Point := @{ a:*; b:*; c:*; };
Cycle := @\x:Point => @\y:Point => {
	aa:* Point.a Point.a;
	bb:* Point.b Point.b;
	cc:* Point.c Point.c;
	ab:* Point.a Point.b;
	bc:* Point.b Point.c;
	ca:* Point.c Point.a;
};
cycle_le := \x:Point => x
	@a => (\y:Point => y @a => Bool.true @b => Bool.true @c => Bool.false)
	@b => (\y:Point => y @a => Bool.false @b => Bool.true @c => Bool.true)
	@c => (\y:Point => y @a => Bool.true @b => Bool.false @c => Bool.true);
cycle_decide := \x:Point => x
	@(self => (y:Point)->general_decision Point Cycle self y (cycle_le self y))
	@a => (\y:Point => y @(self => general_decision Point Cycle Point.a self (cycle_le Point.a self))
		@a => (general_decision Point Cycle Point.a Point.a).yes Cycle.aa
		@b => (general_decision Point Cycle Point.a Point.b).yes Cycle.ab
		@c => (general_decision Point Cycle Point.a Point.c).no Cycle.ca)
	@b => (\y:Point => y @(self => general_decision Point Cycle Point.b self (cycle_le Point.b self))
		@a => (general_decision Point Cycle Point.b Point.a).no Cycle.ab
		@b => (general_decision Point Cycle Point.b Point.b).yes Cycle.bb
		@c => (general_decision Point Cycle Point.b Point.c).yes Cycle.bc)
	@c => (\y:Point => y @(self => general_decision Point Cycle Point.c self (cycle_le Point.c self))
		@a => (general_decision Point Cycle Point.c Point.a).yes Cycle.ca
		@b => (general_decision Point Cycle Point.c Point.b).no Cycle.bc
		@c => (general_decision Point Cycle Point.c Point.c).yes Cycle.cc);
cycle_decide :: (x:Point)->(y:Point)->general_decision Point Cycle x y (cycle_le x y);

cycle_correct := \xs:List Point => quick_locally_sorted Point Cycle (&cycle_le) (&cycle_decide) xs;
cycle_correct :: (xs:List Point)->general_locally_sorted Point Cycle (quickSort Point (&cycle_le) xs);

read_local := \xs:List Point => \p:general_locally_sorted Point Cycle xs => p
	@nil => (List Point).nil
	@one x => (List Point).cons x (List Point).nil
	@cons x y ys edge tail => (List Point).cons x *tail;
empty := (List Point).nil;
input := (List Point).cons Point.a ((List Point).cons Point.b ((List Point).cons Point.c empty));
local_certificate := (general_locally_sorted Point Cycle).cons Point.a Point.b
	((List Point).cons Point.c empty) Cycle.ab
	((general_locally_sorted Point Cycle).cons Point.b Point.c empty Cycle.bc
		((general_locally_sorted Point Cycle).one Point.c));
local_certificate :: general_locally_sorted Point Cycle input;
expected := (List Point).cons Point.c ((List Point).cons Point.a ((List Point).cons Point.b empty));
result := quickSort Point (&cycle_le) input;
certified := read_local result (cycle_correct input);
empty_certified := read_local (quickSort Point (&cycle_le) empty) (cycle_correct empty);
singleton := (List Point).cons Point.a empty;
singleton_certified := read_local (quickSort Point (&cycle_le) singleton) (cycle_correct singleton);
duplicates := (List Point).cons Point.a singleton;
duplicates_certified := read_local (quickSort Point (&cycle_le) duplicates) (cycle_correct duplicates);
