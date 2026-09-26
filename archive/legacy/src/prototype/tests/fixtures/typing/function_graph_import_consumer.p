graphOutput := \input : NatList => \output : Nat =>
	\graph : @length input output => output;

certified := *length NatList.nil;
main := length NatList.nil;
