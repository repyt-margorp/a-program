/* This must fail.
 * The *x induction-hypothesis syntax may only refer to a pattern binder that
 * came from a recursive SELF field of the current match constructor.
 * Here x and y are Nat fields inside PairNat.pair, not recursive PairNat fields.
 */

Nat := @{
	zero : *;
	succ : * -> *;
};

PairNat := @{
	pair : Nat -> Nat -> *;
};

bad := \p : PairNat =>
	p @pair x y => *x;
