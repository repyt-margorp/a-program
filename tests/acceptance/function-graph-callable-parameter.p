Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
List := @{nil:*; cons:Nat->*->*;};
apply := \f:Nat->Nat => \x:Nat => f x;
map := \f:Nat->Nat => \xs:List => xs
	@nil => List.nil
	@cons head tail => List.cons (f head) *tail;
one := Nat.succ Nat.zero;
two := Nat.succ one;
sample := List.cons Nat.zero (List.cons one List.nil);
expected := List.cons one (List.cons two List.nil);
applyGraph := @apply;
mapGraph := @map;
applyMain := apply &Nat.succ one;
main := map &Nat.succ sample;

SameLength := @\xs:List => @\ys:List => {
	nil : * List.nil List.nil;
	cons : (head:Nat) -> (tail:List) -> (mappedHead:Nat) -> (mappedTail:List) ->
		* tail mappedTail -> * (List.cons head tail) (List.cons mappedHead mappedTail);
};
mapPreservesLength := \f:Nat->Nat => \xs:List => \ys:List => \trace:@map f xs ys => trace
	@nil => SameLength.nil
	@cons head tail mappedTail tailGraph mappedHead headGraph =>
		SameLength.cons head tail mappedHead mappedTail *tailGraph;
mapPreservesLength :: (f:Nat->Nat) -> (xs:List) -> (ys:List) -> @map f xs ys -> SameLength xs ys;
map_length := \f:Nat->Nat => \xs:List => xs @(self => SameLength self (map &f self))
	@nil => SameLength.nil
	@cons head tail => SameLength.cons head tail (f head) (map &f tail) *tail;
map_length :: (f:Nat->Nat)->(xs:List)->SameLength xs (map &f xs);
readLength := \xs:List => \ys:List => \proof:SameLength xs ys => proof
	@nil => Nat.zero
	@cons head tail mappedHead mappedTail rest => Nat.succ *rest;
propertyMain := readLength sample (map &Nat.succ sample) (map_length &Nat.succ sample);

filter := \test:Nat->Bool => \xs:List => xs
	@nil => List.nil
	@cons head tail => (test head @true => List.cons head *tail @false => *tail);
isZero := \n:Nat => n @zero => Bool.true @succ k => Bool.false;
always := \n:Nat => Bool.true;
never := \n:Nat => Bool.false;
filterGraph := @filter;
filterMain := filter &isZero sample;
filterExpected := List.cons Nat.zero List.nil;
allMain := filter &always sample;
noneMain := filter &never sample;
noneExpected := List.nil;
