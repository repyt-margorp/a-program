import Nat;
import LT;
import ltLift;

Box := \A : @ => @{box : A -> *;};
wrap := \m : Nat => \n : Nat => \input : Box (LT m (Nat.succ n)) =>
	input @box p => (Box (LT (Nat.succ m) (Nat.succ (Nat.succ n)))).box
		(ltLift m (Nat.succ n) p);
graph := @wrap;
