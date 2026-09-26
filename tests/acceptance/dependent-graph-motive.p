Nat := @{zero : *; succ : * -> *;};
NatList := @{nil : *; cons : Nat -> * -> *;};
length := \xs : NatList =>
	xs @nil => Nat.zero @cons head tail => Nat.succ *tail;
length_graph := \xs:NatList => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons head tail => (@length).cons head tail (length tail) *tail;
length_graph :: (xs:NatList)->@length xs (length xs);

Unary := @\value : Nat => {
	zero : * Nat.zero;
	succ : (predecessor : Nat) -> * predecessor -> * (Nat.succ predecessor);
};

lengthOutputUnary := \input : NatList => \output : Nat =>
	\graph : @length input output => graph
		@nil => Unary.zero
		@cons head tail tailLength tailGraph => Unary.succ tailLength *tailGraph;

reordered := \input : NatList => \output : Nat =>
	\graph : @length input output => graph
		@cons head tail tailLength tailGraph => Unary.succ tailLength *tailGraph
		@nil => Unary.zero;

one := NatList.cons Nat.zero NatList.nil;
two := NatList.cons Nat.zero one;
oneLength := Nat.succ Nat.zero;
twoLength := Nat.succ oneLength;
nilGraph := (@length).nil;
oneGraph := (@length).cons Nat.zero NatList.nil Nat.zero nilGraph;
twoGraph := (@length).cons Nat.zero one oneLength oneGraph;

main := lengthOutputUnary two twoLength twoGraph;
expected := Unary.succ oneLength (Unary.succ Nat.zero Unary.zero);
main :: Unary twoLength;
reorderedMain := reordered two twoLength twoGraph;
certifiedMain := lengthOutputUnary two (length two) (length_graph two);
certifiedMain :: Unary twoLength;
