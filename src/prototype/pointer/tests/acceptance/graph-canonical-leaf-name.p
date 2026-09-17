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
base := *f Input.case1 @output => read Input.case1 output @output;
main := *f Input.other @output => read Input.other output @output;
