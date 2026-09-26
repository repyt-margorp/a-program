/* Supply computed-index-arithmetic.p with --imports. */
import Nat;
import Vec;
import add;

prepend := \m:Nat => \n:Nat => \xs:Vec Nat (add m n) =>
	((Vec Nat).cons (add m n) Nat.zero xs :: Vec Nat (add (Nat.succ m) n));
