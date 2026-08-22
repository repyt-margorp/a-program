Nat := @{
	zero : *;
	succ : * -> *;
};

NatList := @{
	nil : *;
	cons : Nat -> * -> *;
};

LengthGraph := @\input : NatList => @\output : Nat => {
	nilCase : * NatList.nil Nat.zero;
};

LengthResult := @\input : NatList => {
	returned : (actualInput : NatList) -> (output : Nat) ->
		LengthGraph actualInput output -> * actualInput;
};

emptyPacket := LengthResult.returned
	NatList.nil Nat.zero LengthGraph.nilCase;

emptyPacket :: LengthResult NatList.nil;

project := \input : NatList => \packet : LengthResult input =>
	packet @returned actualInput output graph => output;

main := project NatList.nil emptyPacket;
