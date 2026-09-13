import Bool;
import Nat;
import Precedes;
import Acc;
import precedesRelation;
import accElim;
import accFalse;

accTrue := (Acc Bool &precedesRelation).acc Bool.true
	&(\y:Bool => \edge:Precedes y Bool.true => edge @falseBeforeTrue => accFalse);
heightStep := \b:Bool => \ih:(y:Bool)->Precedes y b->Nat => b
	@false => Nat.zero
	@true => Nat.succ (ih Bool.false Precedes.falseBeforeTrue);
natFamily := \b:Bool => Nat;
main := accElim Bool &precedesRelation &natFamily &heightStep Bool.true accTrue;
expected := Nat.succ Nat.zero;
base := accElim Bool &precedesRelation &natFamily &heightStep Bool.false accFalse;
zero := Nat.zero;
