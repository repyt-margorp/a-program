import Nat;
import LT;
import ltLift;

Box := \A : @ => @{box : A -> *;};
wrap := \m : Nat => \n : Nat => \input : Box (LT m n) =>
	input @box p => (Box (LT (Nat.succ m) (Nat.succ n))).box (ltLift m n p);
graph := @wrap;
