import Bool;
import Nat;
import Precedes;
import Acc;
import precedesRelation;
import accElim;
import accFalse;

falseMain := accFalse @acc subject down => subject;
falseExpected := Bool.false;

constantStep := \b:Bool => \ih:(y:Bool)->Precedes y b->Nat => Nat.succ Nat.zero;
natFamily := \b:Bool => Nat;
main := accElim Bool &precedesRelation &natFamily &constantStep Bool.false accFalse;
expected := Nat.succ Nat.zero;
