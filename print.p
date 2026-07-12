Nat := @{
	zero : *;
	succ : * -> *;
};

from_intrinsic :: #.Nat -> Nat;
from_intrinsic := \n : #.Nat =>
	n @zero		=> Nat.zero
	  @succ k	=> Nat.succ (*k);

to_intrinsic :: Nat -> #.Nat;
to_intrinsic := \n : Nat =>
	n @zero		=> #.Nat.zero
	  @succ k	=> #.Nat.succ (*k);

add :: Nat -> Nat -> Nat;
add := \n : Nat =>
	n @zero		=> (\m : Nat => m)
	  @succ k	=> (\m : Nat => Nat.succ (*k m));

mul :: Nat -> Nat -> Nat;
mul := \n : Nat =>
	n @zero		=> (\m : Nat => Nat.zero)
	  @succ k	=> (\m : Nat => add m (*k m));

x := from_intrinsic (#.code_to_nat #"53");
y := from_intrinsic (#.code_to_nat #"27");

main := #.print (#.nat_to_code (to_intrinsic (mul x y)));
