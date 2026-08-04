Nat := @{ zero : *; succ : * -> *; };
List := \A : @ => @{ nil : *; cons : A -> * -> *; };

headOrZero := \xs : List Nat =>
	xs @nil => Nat.zero
	   @cons x rest => x;
input := (List Nat).cons (Nat.succ Nat.zero) (List Nat).nil;
expected := { Nat.succ Nat.zero; };
main := headOrZero input;
