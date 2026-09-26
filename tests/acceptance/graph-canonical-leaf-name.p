Nat := @{ zero : *; succ : * -> *; };
Input := @{ case1 : *; other : *; };
f := \x : Input => x @case1 => Nat.zero @other => Nat.succ Nat.zero;
first := (@f).case0;
second := (@f).case1;
first :: @f Input.case1 Nat.zero;
second :: @f Input.other (Nat.succ Nat.zero);
read := \x : Input => \y : Nat => \g : @f x y => g
	@case0 => Nat.zero @case1 => Nat.succ Nat.zero;
zero := Nat.zero;
one := Nat.succ zero;
base := read Input.case1 (f Input.case1) first;
main := read Input.other (f Input.other) second;
