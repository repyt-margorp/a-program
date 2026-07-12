/* This must fail.
 *
 * The two base cases constrain the match motive to different result types.
 * The recursive branch requires the IH result to be Nat because Nat.succ
 * consumes it. A solver must not turn this into a constant motive candidate.
 */

Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

BadNat := @{
	zero0 : *;
	zero1 : *;
	succ : * -> *;
};

bad := \n : BadNat =>
	n @zero0 => Bool.true
	  @zero1 => Nat.zero
	  @succ k => Nat.succ *k;
