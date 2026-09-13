Nat := @{ zero : *; succ : * -> *; };
At := @\n : Nat => { at : (k : Nat) -> * k; };
Box := \A : @ => @{ box : *; };

build := \n : Nat =>
	n @zero => (\m : Nat => (Box (At Nat.zero)).box)
	  @succ k => (\m : Nat => (Box (At (Nat.succ k))).box);
build :: (n : Nat) -> Nat -> Box (At n);

main := build (Nat.succ Nat.zero) Nat.zero;
expected := (Box (At (Nat.succ Nat.zero))).box;

copy := \box : Nat =>
	box @zero => Nat.zero
	    @succ k => ((Box Nat).box @box => Nat.succ (*k));
recursiveMain := copy (Nat.succ (Nat.succ Nat.zero));
recursiveExpected := Nat.succ (Nat.succ Nat.zero);
