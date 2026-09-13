Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
LengthGraph := @\input:NatList => @\output:Nat => {
	nilCase:* NatList.nil Nat.zero;
	consCase:(head:Nat)->(tail:NatList)->(tailLength:Nat)->
		* tail tailLength -> * (NatList.cons head tail) (Nat.succ tailLength);
};
LengthResult := @\input:NatList => {
	returned:(actualInput:NatList)->(output:Nat)->LengthGraph actualInput output->* actualInput;
};

extend := \head:Nat => \tail:NatList => \recursive:LengthResult tail =>
	recursive @returned recursiveInput tailLength graph =>
		LengthResult.returned (NatList.cons head tail) (Nat.succ tailLength)
			(LengthGraph.consCase head tail tailLength (graph :: LengthGraph tail tailLength));
lengthCertified := \xs:NatList => xs
	@nil => LengthResult.returned NatList.nil Nat.zero LengthGraph.nilCase
	@cons head tail => extend head tail *tail;
lengthCertified :: (xs:NatList)->LengthResult xs;
length := \xs:NatList => {
	certified := lengthCertified xs;
	certified @returned actualInput output graph => output;
};
length :: NatList->Nat;
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
sample := NatList.cons zero (NatList.cons one NatList.nil);
main := length sample;
base := length NatList.nil;
singleton := length (NatList.cons zero NatList.nil);

eraseCertified := \xs:NatList => xs
	@nil => LengthResult.returned NatList.nil Nat.zero LengthGraph.nilCase
	@cons head tail => {
		ignored := *tail;
		LengthResult.returned NatList.nil Nat.zero LengthGraph.nilCase;
	};
eraseCertified :: NatList->LengthResult NatList.nil;
erased := eraseCertified sample;
emptyResult := LengthResult.returned NatList.nil Nat.zero LengthGraph.nilCase;

Tree := @{leaf:*; fork:*->*->*; skip:*->*;};
Tagged := @\tree:Tree => {at:(tree:Tree)->* tree;};
tag := \tree:Tree => tree
	@leaf => Tagged.at Tree.leaf
	@fork left right => {visited:=*left; Tagged.at (Tree.fork left right);}
	@skip child => Tagged.at (Tree.skip child);
tag :: (tree:Tree)->Tagged tree;
tree := Tree.fork (Tree.skip Tree.leaf) Tree.leaf;
tagged := tag tree;
expectedTag := Tagged.at tree;
skipped := tag (Tree.skip Tree.leaf);
expectedSkip := Tagged.at (Tree.skip Tree.leaf);
