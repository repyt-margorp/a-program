import Bool;
import Precedes;
import Acc;
import precedesRelation;

bad := (Acc Bool &precedesRelation).acc Bool.true
	&(\y:Bool => \edge:Precedes y Bool.true => edge @falseBeforeTrue => Bool.true);
