Nat := @{ zero : *; succ : * -> *; };
f := \x : Nat => x
	@zero => Nat.zero
	@succ n => (n @zero => Nat.zero @succ k => Nat.succ k);
main := @f;
Result := @\input : Nat => @\output : Nat => {
	zero : * Nat.zero Nat.zero;
	one : * (Nat.succ Nat.zero) Nat.zero;
	larger : (k : Nat) -> * (Nat.succ (Nat.succ k)) (Nat.succ k);
};
correct := \x : Nat => \y : Nat => \proof : @f x y => proof
	@case0 => Result.zero
	@case1 => Result.one
	@case2 k => Result.larger k;
correct :: (x : Nat) -> (y : Nat) -> @f x y -> Result x y;
read := \x : Nat => \y : Nat => \proof : Result x y => proof
	@zero => Nat.zero
	@one => Nat.zero
	@larger k => Nat.succ k;
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
base := read zero (f zero) (correct zero (f zero) firstLeaf);
middle := read one (f one) (correct one (f one) secondLeaf);
last := read two (f two) (correct two (f two) thirdLeaf);
firstLeaf := (@f).case0;
secondLeaf := (@f).case1;
thirdLeaf := (@f).case2 zero;
firstLeaf :: @f zero zero;
secondLeaf :: @f one zero;
thirdLeaf :: @f two one;
