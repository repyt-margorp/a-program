Nat := @{ zero : *; succ : * -> *; };
helper := \n : Nat => n @zero => Nat.zero @succ k => Nat.succ k;
f := \x : Nat => x @zero => Nat.zero @succ n => helper n;
main := @f;
read := \x : Nat => \y : Nat => \proof : @f x y => proof
	@case0 => Nat.zero
	@case1 => Nat.zero
	@case2 k => Nat.succ k;
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
base := read zero (f zero) (@f).case0;
middle := read one (f one) (@f).case1;
last := read two (f two) ((@f).case2 zero);
