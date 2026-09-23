import Nat;
import LT;
import ltLift;

Box := \A : @ => @{box : A -> *;};
wrap := \m : Nat => \n : Nat => \input : Box (LT m (Nat.succ n)) =>
	input @box p => (Box (LT (Nat.succ m) (Nat.succ (Nat.succ n)))).box
		(ltLift m (Nat.succ n) p);
graph := @wrap;

read := \m:Nat => \n:Nat => \input:Box (LT m (Nat.succ n)) =>
	\output:Box (LT (Nat.succ m) (Nat.succ (Nat.succ n))) =>
	\proof:@wrap m n input output => proof
		@case0 original lifted trace => lifted;
read :: (m:Nat)->(n:Nat)->(input:Box (LT m (Nat.succ n)))->
	(output:Box (LT (Nat.succ m) (Nat.succ (Nat.succ n))))->
	@wrap m n input output->LT (Nat.succ m) (Nat.succ (Nat.succ n));

zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
first := (Box (LT zero one)).box (LT.step zero);
main := *wrap zero zero first @output => output;
proofMain := *wrap zero zero first @output => read zero zero first output @output;
expected := (Box (LT one two)).box (LT.step one);
proofExpected := LT.step one;

second := (Box (LT zero two)).box (LT.weakenRight zero one (LT.step zero));
recursiveMain := *wrap zero one second @output => output;
recursiveProof := *wrap zero one second @output => read zero one second output @output;
recursiveExpected := (Box (LT one (Nat.succ two))).box (LT.weakenRight one two (LT.step one));
recursiveProofExpected := LT.weakenRight one two (LT.step one);
