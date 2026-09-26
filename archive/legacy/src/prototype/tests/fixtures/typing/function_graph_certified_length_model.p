Nat := @{
	zero : *;
	succ : * -> *;
};

NatList := @{
	nil : *;
	cons : Nat -> * -> *;
};

LengthGraph :=
	@\input : NatList =>
	@\output : Nat =>
	{
		nilCase : * NatList.nil Nat.zero;
		consCase :
			(head : Nat) ->
			(tail : NatList) ->
			(tailLength : Nat) ->
			* tail tailLength ->
			* (NatList.cons head tail) (Nat.succ tailLength);
	};

LengthResult := @\input : NatList => {
	returned : (actualInput : NatList) -> (output : Nat) ->
		LengthGraph actualInput output -> * actualInput;
};

lengthCertified := \xs : NatList =>
	xs
		@nil => LengthResult.returned NatList.nil Nat.zero LengthGraph.nilCase
		@cons head tail => {
			recursive := *tail;
			recursive @returned recursiveInput tailLength graph =>
				LengthResult.returned
					(NatList.cons head tail)
					(Nat.succ tailLength)
					(LengthGraph.consCase head tail tailLength
						(graph :: LengthGraph tail tailLength));
		};

length := \xs : NatList => {
	certified := lengthCertified xs;
		certified @returned actualInput output graph => output;
};

length :: NatList -> Nat;

one := NatList.cons Nat.zero NatList.nil;
main := length one;
