import Nat;
import LT;
import ltLift;

Box := \A : @ => @{box : A -> *;};
wrap := \m : Nat => \n : Nat => \input : Box (LT m (Nat.succ n)) =>
	input @box p => (Box (LT (Nat.succ m) (Nat.succ (Nat.succ n)))).box
		(ltLift m (Nat.succ n) p);
graph := @wrap;
lift_graph := \m:Nat => \n:Nat => \p:LT m n => p
	@(left right self => @ltLift left right self (ltLift left right self))
	@step k => (@ltLift).case0 k
	@weakenRight left right prior => (@ltLift).case1 left right prior
		(ltLift left right prior) *prior;
lift_graph :: (m:Nat)->(n:Nat)->(p:LT m n)->@ltLift m n p (ltLift m n p);

read := \m:Nat => \n:Nat => \input:Box (LT m (Nat.succ n)) =>
	\output:Box (LT (Nat.succ m) (Nat.succ (Nat.succ n))) =>
	\proof:@wrap m n input output => proof
		@case0 original lifted trace => lifted;
read :: (m:Nat)->(n:Nat)->(input:Box (LT m (Nat.succ n)))->
	(output:Box (LT (Nat.succ m) (Nat.succ (Nat.succ n))))->
	@wrap m n input output->LT (Nat.succ m) (Nat.succ (Nat.succ n));
read_value := \m:Nat => \n:Nat => \output:Box (LT m n) => output @box p => p;
read_value :: (m:Nat)->(n:Nat)->Box (LT m n)->LT m n;

zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
first := (Box (LT zero one)).box (LT.step zero);
main := wrap zero zero first;
proofMain := read_value one two (wrap zero zero first);
expected := (Box (LT one two)).box (LT.step one);
proofExpected := LT.step one;

second := (Box (LT zero two)).box (LT.weakenRight zero one (LT.step zero));
recursiveMain := wrap zero one second;
recursiveProof := read_value one (Nat.succ two) (wrap zero one second);
recursiveExpected := (Box (LT one (Nat.succ two))).box (LT.weakenRight one two (LT.step one));
recursiveProofExpected := LT.weakenRight one two (LT.step one);
