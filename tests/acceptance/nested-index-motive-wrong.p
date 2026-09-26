Nat := @{ zero : *; succ : * -> *; };
At := @\n : Nat => { at : (k : Nat) -> * k; };
Box := \A : @ => @{ box : *; };

build := \n : Nat =>
	n @zero => (\m : Nat => (Box (At Nat.zero)).box)
	  @succ k => (\m : Nat => (Box (At (Nat.succ k))).box);

main := build (Nat.succ Nat.zero) Nat.zero;
main :: Box (At Nat.zero);
